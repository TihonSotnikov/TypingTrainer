#include "typing_trainer_adapter.hpp"

#include "text_utils.hpp"
#include "typing_trainer_core.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace typing_trainer
{

namespace
{

constexpr auto K_STATS_FILE       = "ngram_stats.json";
constexpr auto K_CUSTOM_TEXT_FILE = "custom_text.txt";
constexpr auto K_SETTINGS_GROUP   = "session";

/// \brief Сколько символов длинного текста показывать разом. Отрисовка rich text растёт
///        с его длиной, а страница держит стоимость нажатия постоянной.
constexpr std::size_t K_PAGE_SIZE = 2000;

/// \brief Текст для свободного режима, пока пользователь не ввёл свой.
QString defaultCustomText()
{
	return QStringLiteral("Съешь же ещё этих мягких французских булок, да выпей чаю. "
	                      "В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!");
}

/// \brief Свой текст в виде для набора: составная форма Unicode и normalize_text.
/// \note В тексте из PDF или с macOS «й» и «ё» бывают разложены на букву и отдельный
///       знак; без NFC такая буква заняла бы две позиции набора.
QString typeableText(const QString& text)
{
	return QString::fromStdU32String(
	    normalize_text(text.normalized(QString::NormalizationForm_C).toStdU32String()));
}

/// \brief Каталог пользовательских данных (создаётся при необходимости).
/// \note Переменная окружения TYPING_TRAINER_DATA_DIR задаёт свой каталог (портативный
///       режим, тесты). До версии 1.1 статистика писалась в текущий каталог процесса:
///       если в новом месте файла ещё нет, старый файл переносится (копируется) оттуда.
QString prepareDataLocation()
{
	QString const override_dir = qEnvironmentVariable("TYPING_TRAINER_DATA_DIR");
	if (!override_dir.isEmpty())
	{
		QDir().mkpath(override_dir);
		return QDir(override_dir).absolutePath();
	}

	QString const dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	QDir().mkpath(dir);

	QString const target = QDir(dir).filePath(QString::fromLatin1(K_STATS_FILE));
	if (!QFile::exists(target))
	{
		for (QString const& legacy_dir :
		     QStringList{QDir::currentPath(), QCoreApplication::applicationDirPath()})
		{
			QString const legacy = QDir(legacy_dir).filePath(QString::fromLatin1(K_STATS_FILE));
			if (QFile::exists(legacy) && QFile::copy(legacy, target)) break;
		}
	}

	return dir;
}

QString languageCode(Language language)
{ return language == Language::Russian ? QStringLiteral("ru") : QStringLiteral("en"); }

QString languageCode(char32_t letter)
{ return script_of(letter) == Script::Cyrillic ? QStringLiteral("ru") : QStringLiteral("en"); }

QVariantMap toVariant(const NgramReport& report)
{
	return {
	    {"gram", QString::fromStdU32String(report.gram)},
	    {"avgTime", report.avg_time * 1000.0}, // мс
	    {"errorRate", report.error_rate},
	    {"attempts", static_cast<qulonglong>(report.attempts)},
	    {"language",
		 report.gram.empty() ? QStringLiteral("en") : languageCode(report.gram.front())},
	};
}

QVariantMap toVariant(const SessionRecord& record)
{
	return {
	    {"finishedAt", QDateTime::fromSecsSinceEpoch(record.finished_at)},
	    {"mode",
		 record.mode == TrainingMode::Smart ? QStringLiteral("smart") : QStringLiteral("free")},
	    {"language", languageCode(record.language)},
	    {"length", static_cast<qulonglong>(record.length)},
	    {"errors", static_cast<qulonglong>(record.errors)},
	    {"wpm", record.metrics.wpm},
	    {"cpm", record.metrics.cpm},
	    {"accuracy", record.metrics.accuracy},
	    {"consistency", record.metrics.consistency},
	    {"duration", record.metrics.elapsed_seconds},
	};
}

bool isControlCharacter(char32_t ch) { return ch < 0x20 || (ch >= 0x7F && ch <= 0x9F); }

} // namespace

QmlTypingTrainerAdapter::QmlTypingTrainerAdapter(QObject* parent) :
    QObject(parent), data_location_(prepareDataLocation()),
    data_dir_(data_location_.toStdU16String())
{
	loadSettings();

	core_ = std::make_unique<TypingTrainerCore>(data_dir_);
	core_->set_output_ready_callback([this] {
		// Колбэк вызывается из рабочего потока ядра - переносим обработку в UI-поток.
		QMetaObject::invokeMethod(this, &QmlTypingTrainerAdapter::onOutputReady,
		                          Qt::QueuedConnection);
	});

	showPreview();
}

QmlTypingTrainerAdapter::~QmlTypingTrainerAdapter()
{
	// Ядро ещё может дёрнуть колбэк из рабочего потока, пока останавливается.
	core_->set_output_ready_callback(nullptr);
}

// ----- Управление сессией -----

void QmlTypingTrainerAdapter::start()
{
	// Набирать нечего - прежняя тренировка не должна остаться активной под видом своего текста.
	if (!smart_mode_ && custom_text_.isEmpty())
	{
		stop();
		return;
	}
	core_->push_input(StartSessionCommand{.config = sessionConfig()});
}

void QmlTypingTrainerAdapter::restart() { core_->push_input(RestartSessionCommand{}); }

void QmlTypingTrainerAdapter::stop() { core_->push_input(StopSessionCommand{}); }

void QmlTypingTrainerAdapter::pause() { core_->push_input(PauseSessionCommand{}); }

void QmlTypingTrainerAdapter::resume() { core_->push_input(ResumeSessionCommand{}); }

void QmlTypingTrainerAdapter::typeText(const QString& text)
{
	auto const now = std::chrono::steady_clock::now();
	for (char32_t const ch : text.toStdU32String())
	{
		if (isControlCharacter(ch)) continue;
		core_->push_input(KeyPressData{.key = ch, .timestamp = now});
	}
}

void QmlTypingTrainerAdapter::backspace()
{
	core_->push_input(
	    KeyPressData{.key = ControlKey::Backspace, .timestamp = std::chrono::steady_clock::now()});
}

void QmlTypingTrainerAdapter::requestStatistics() { core_->push_input(RequestStatisticsCommand{}); }

void QmlTypingTrainerAdapter::resetStatistics() { core_->push_input(ResetStatisticsCommand{}); }

SessionConfig QmlTypingTrainerAdapter::sessionConfig() const
{
	SessionConfig config;
	config.mode          = smart_mode_ ? TrainingMode::Smart : TrainingMode::Free;
	config.custom_text   = custom_text_.toStdU32String();
	config.ignore_case   = ignore_case_;
	config.language      = russian_ ? Language::Russian : Language::English;
	config.filler_ratio  = 1.0 - difficulty_; // сложность = доля слов с проблемными сочетаниями
	config.target_length = static_cast<std::size_t>(target_length_);
	return config;
}

// ----- События ядра -----

void QmlTypingTrainerAdapter::onOutputReady()
{
	bool text_changed = false;

	while (auto event = core_->poll_output())
	{
		std::visit(
		    [this, &text_changed](auto&& arg) {
			    using T = std::decay_t<decltype(arg)>;
			    if constexpr (std::is_same_v<T, SessionState>)
			    {
				    applyState(arg);
				    text_changed = true;
			    }
			    else if constexpr (std::is_same_v<T, StateUpdate>)
			    {
				    applyUpdate(arg);
				    text_changed = true;
			    }
			    else if constexpr (std::is_same_v<T, SessionResult>) { applyResult(arg); }
			    else if constexpr (std::is_same_v<T, StatisticsSnapshot>) { applyStatistics(arg); }
			    else if constexpr (std::is_same_v<T, LayoutMismatch>)
			    {
				    emit layoutMismatch(languageCode(arg.expected));
			    }
		    },
		    *event);
	}

	// Пачку событий (быстрый набор) отрисовываем один раз.
	if (text_changed) rebuildFormattedText();
}

void QmlTypingTrainerAdapter::applyState(const SessionState& state)
{
	chars_           = state.chars;
	cursor_position_ = static_cast<int>(state.cursor_position);
	splitPages();
	setMetrics(state.metrics);
	setStatus(state.status);

	// Сессию остановили - показываем текст, который будет набираться дальше.
	if (state.status == SessionStatus::Inactive) showPreview();

	emit cursorPositionChanged();
}

void QmlTypingTrainerAdapter::applyUpdate(const StateUpdate& update)
{
	if (update.changed_index < chars_.size()) chars_.at(update.changed_index) = update.changed_char;

	// После последнего символа курсор встаёт за концом текста.
	cursor_position_ = update.is_completed ? static_cast<int>(chars_.size())
	                                       : static_cast<int>(update.cursor_position);
	selectPage();
	setMetrics(update.metrics);
	setStatus(update.status);

	emit cursorPositionChanged();
}

void QmlTypingTrainerAdapter::applyResult(const SessionResult& result)
{
	QVariantList weakest;
	for (auto const& report : result.weakest)
		weakest.append(toVariant(report));

	last_result_ = toVariant(result.record);
	last_result_.insert(QStringLiteral("weakest"), weakest);
	last_result_.insert(QStringLiteral("personalBest"), result.is_personal_best);

	emit sessionFinished();
}

void QmlTypingTrainerAdapter::applyStatistics(const StatisticsSnapshot& snapshot)
{
	weak_ngrams_.clear();
	for (auto const& report : snapshot.weakest)
		weak_ngrams_.append(toVariant(report));

	history_.clear();
	for (auto const& record : snapshot.history)
		history_.append(toVariant(record));

	emit statisticsChanged();
}

void QmlTypingTrainerAdapter::setStatus(SessionStatus status)
{
	auto const mapped = static_cast<Status>(status);
	if (status_ == mapped) return;
	status_ = mapped;
	emit statusChanged();
}

void QmlTypingTrainerAdapter::setMetrics(const SessionMetrics& metrics)
{
	metrics_ = metrics;
	emit metricsChanged();
}

// ----- Подсветка текста -----

void QmlTypingTrainerAdapter::showPreview()
{
	chars_.clear();
	cursor_position_ = 0;

	if (!smart_mode_)
		for (char32_t const ch : custom_text_.toStdU32String())
			chars_.push_back(CharState{.character = ch, .status = CharStatus::Pending});

	splitPages();
	rebuildFormattedText();
	emit cursorPositionChanged();
}

void QmlTypingTrainerAdapter::splitPages()
{
	page_starts_.assign(1, 0);
	for (std::size_t start = 0; chars_.size() - start > K_PAGE_SIZE;)
	{
		// Граница страницы - сразу после пробела, чтобы не разрывать слово.
		std::size_t next = start + K_PAGE_SIZE;
		while (next < chars_.size() && chars_.at(next - 1).character != U' ')
			++next;
		if (next >= chars_.size()) break;
		page_starts_.push_back(next);
		start = next;
	}
	selectPage();
}

void QmlTypingTrainerAdapter::selectPage()
{
	// Страница, на которой стоит курсор; курсор за концом текста - последняя страница.
	std::size_t const last = chars_.empty() ? 0 : chars_.size() - 1;
	std::size_t const index
	    = std::min(static_cast<std::size_t>(std::max(cursor_position_, 0)), last);

	auto const page = std::ranges::upper_bound(page_starts_, index) - 1;
	page_start_     = *page;
	page_end_       = (std::next(page) != page_starts_.end()) ? *std::next(page) : chars_.size();
}

void QmlTypingTrainerAdapter::rebuildFormattedText()
{
	QString const pending  = pending_color_.name();
	QString const correct  = correct_color_.name();
	QString const wrong    = wrong_color_.name();
	QString const wrong_bg = wrong_background_.name();

	// Соседние символы с одинаковым статусом идут одним span: HTML остаётся компактным
	// даже для длинных текстов. pre-wrap не даёт схлопнуть пробелы - иначе курсор
	// разойдётся с позицией символа.
	QString html = QStringLiteral("<span style=\"white-space: pre-wrap;\">");
	html.reserve((static_cast<qsizetype>(page_end_ - page_start_) * 2) + 256);

	std::size_t i = page_start_;
	while (i < page_end_)
	{
		CharStatus const status = chars_.at(i).status;
		std::u32string   run;
		for (; i < page_end_ && chars_.at(i).status == status; ++i)
			run.push_back(chars_.at(i).character);

		QString const text = QString::fromStdU32String(run).toHtmlEscaped();
		switch (status)
		{
		case CharStatus::Pending:
			html += QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(pending, text);
			break;
		case CharStatus::Correct:
			html += QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(correct, text);
			break;
		case CharStatus::Wrong:
			html += QStringLiteral("<span style=\"color:%1;background-color:%2;\">%3</span>")
			            .arg(wrong, wrong_bg, text);
			break;
		}
	}
	html += QStringLiteral("</span>");

	if (formatted_text_ == html) return;
	formatted_text_ = std::move(html);
	emit formattedTextChanged();
}

// ----- Настройки -----

QString QmlTypingTrainerAdapter::language() const
{ return russian_ ? QStringLiteral("ru") : QStringLiteral("en"); }

void QmlTypingTrainerAdapter::setSmartMode(bool enabled)
{
	if (smart_mode_ == enabled) return;
	smart_mode_ = enabled;
	saveSetting(QStringLiteral("smartMode"), enabled);
	emit smartModeChanged();
	if (status_ == Status::Inactive) showPreview();
}

void QmlTypingTrainerAdapter::setLanguage(const QString& code)
{
	bool const russian = (code == QStringLiteral("ru"));
	if (russian_ == russian) return;
	russian_ = russian;
	saveSetting(QStringLiteral("language"), language());
	emit languageChanged();
}

void QmlTypingTrainerAdapter::setDifficulty(double value)
{
	value = std::clamp(value, 0.0, 1.0);
	if (qFuzzyCompare(difficulty_, value)) return;
	difficulty_ = value;
	saveSetting(QStringLiteral("difficulty"), value);
	emit difficultyChanged();
}

void QmlTypingTrainerAdapter::setTargetLength(int length)
{
	length = std::clamp(length, K_MIN_TARGET_LENGTH, K_MAX_TARGET_LENGTH);
	if (target_length_ == length) return;
	target_length_ = length;
	saveSetting(QStringLiteral("targetLength"), length);
	emit targetLengthChanged();
}

void QmlTypingTrainerAdapter::setIgnoreCase(bool enabled)
{
	if (ignore_case_ == enabled) return;
	ignore_case_ = enabled;
	saveSetting(QStringLiteral("ignoreCase"), enabled);
	emit ignoreCaseChanged();
}

void QmlTypingTrainerAdapter::setAutoPause(bool enabled)
{
	if (auto_pause_ == enabled) return;
	auto_pause_ = enabled;
	saveSetting(QStringLiteral("autoPause"), enabled);
	emit autoPauseChanged();
}

void QmlTypingTrainerAdapter::setCustomText(const QString& text)
{
	// Храним уже нормализованный текст: в редакторе видно ровно то, что придётся набирать.
	QString const normalized = typeableText(text);
	if (custom_text_ == normalized) return;
	custom_text_ = normalized;
	saveCustomText();
	emit customTextChanged();
	if (status_ == Status::Inactive) showPreview();
}

void QmlTypingTrainerAdapter::loadSettings()
{
	QSettings settings;
	settings.beginGroup(QString::fromLatin1(K_SETTINGS_GROUP));
	smart_mode_ = settings.value(QStringLiteral("smartMode"), smart_mode_).toBool();
	russian_    = settings.value(QStringLiteral("language"), language()).toString() == u"ru";
	difficulty_ = std::clamp(settings.value(QStringLiteral("difficulty"), difficulty_).toDouble(),
	                         0.0, 1.0);
	target_length_
	    = std::clamp(settings.value(QStringLiteral("targetLength"), target_length_).toInt(),
	                 K_MIN_TARGET_LENGTH, K_MAX_TARGET_LENGTH);
	ignore_case_ = settings.value(QStringLiteral("ignoreCase"), ignore_case_).toBool();
	auto_pause_  = settings.value(QStringLiteral("autoPause"), auto_pause_).toBool();
	settings.endGroup();

	// Файл могла записать версия, ещё не приводившая текст к составной форме.
	QFile file(QDir(data_location_).filePath(QString::fromLatin1(K_CUSTOM_TEXT_FILE)));
	custom_text_ = file.open(QIODevice::ReadOnly) ? typeableText(QString::fromUtf8(file.readAll()))
	                                              : defaultCustomText();
}

void QmlTypingTrainerAdapter::saveSetting(const QString& key, const QVariant& value)
{
	QSettings settings;
	settings.beginGroup(QString::fromLatin1(K_SETTINGS_GROUP));
	settings.setValue(key, value);
}

void QmlTypingTrainerAdapter::saveCustomText() const
{
	// Свой текст может быть длинным - храним его файлом рядом со статистикой, а не в QSettings.
	QSaveFile file(QDir(data_location_).filePath(QString::fromLatin1(K_CUSTOM_TEXT_FILE)));
	if (!file.open(QIODevice::WriteOnly)) return;
	file.write(custom_text_.toUtf8());
	file.commit();
}

// ----- Цвета -----

void QmlTypingTrainerAdapter::setPendingColor(const QColor& color)
{
	if (pending_color_ == color) return;
	pending_color_ = color;
	emit colorsChanged();
	rebuildFormattedText();
}

void QmlTypingTrainerAdapter::setCorrectColor(const QColor& color)
{
	if (correct_color_ == color) return;
	correct_color_ = color;
	emit colorsChanged();
	rebuildFormattedText();
}

void QmlTypingTrainerAdapter::setWrongColor(const QColor& color)
{
	if (wrong_color_ == color) return;
	wrong_color_ = color;
	emit colorsChanged();
	rebuildFormattedText();
}

void QmlTypingTrainerAdapter::setWrongBackground(const QColor& color)
{
	if (wrong_background_ == color) return;
	wrong_background_ = color;
	emit colorsChanged();
	rebuildFormattedText();
}

} // namespace typing_trainer
