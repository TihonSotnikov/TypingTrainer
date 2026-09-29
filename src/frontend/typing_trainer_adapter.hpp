#pragma once

#include "contracts.hpp"

#include <QColor>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace typing_trainer
{

/// \brief Мост между QML-интерфейсом и ядром тренажёра.
///
/// Доступен в QML как синглтон Trainer. Переводит действия пользователя в InputEvent,
/// а события ядра - в свойства и сигналы. Настройки сессии сохраняются между запусками.
class QmlTypingTrainerAdapter : public QObject
{
	Q_OBJECT
	QML_NAMED_ELEMENT(Trainer)
	QML_SINGLETON

	// ----- Текущая сессия -----
	Q_PROPERTY(Status status READ status NOTIFY statusChanged)
	Q_PROPERTY(QString formattedText READ formattedText NOTIFY formattedTextChanged)
	Q_PROPERTY(int textLength READ textLength NOTIFY formattedTextChanged)
	/// Индекс первого символа, показанного в formattedText (длинный текст выводится страницами).
	Q_PROPERTY(int displayOffset READ displayOffset NOTIFY formattedTextChanged)
	/// Позиция курсора в formattedText: от начала страницы, в единицах UTF-16 документа.
	Q_PROPERTY(int displayCursorPosition READ displayCursorPosition NOTIFY formattedTextChanged)
	Q_PROPERTY(int cursorPosition READ cursorPosition NOTIFY cursorPositionChanged)
	Q_PROPERTY(bool typingStarted READ typingStarted NOTIFY metricsChanged)

	// ----- Метрики -----
	Q_PROPERTY(double wpm READ wpm NOTIFY metricsChanged)
	Q_PROPERTY(double cpm READ cpm NOTIFY metricsChanged)
	Q_PROPERTY(double accuracy READ accuracy NOTIFY metricsChanged)
	Q_PROPERTY(double consistency READ consistency NOTIFY metricsChanged)
	Q_PROPERTY(double elapsed READ elapsed NOTIFY metricsChanged)

	// ----- Итоги и статистика -----
	Q_PROPERTY(QVariantMap lastResult READ lastResult NOTIFY sessionFinished)
	Q_PROPERTY(QVariantList weakNgrams READ weakNgrams NOTIFY statisticsChanged)
	Q_PROPERTY(QVariantList history READ history NOTIFY statisticsChanged)

	// ----- Настройки (сохраняются между запусками) -----
	Q_PROPERTY(bool smartMode READ smartMode WRITE setSmartMode NOTIFY smartModeChanged)
	Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
	Q_PROPERTY(double difficulty READ difficulty WRITE setDifficulty NOTIFY difficultyChanged)
	Q_PROPERTY(int targetLength READ targetLength WRITE setTargetLength NOTIFY targetLengthChanged)
	Q_PROPERTY(int minTargetLength READ minTargetLength CONSTANT)
	Q_PROPERTY(int maxTargetLength READ maxTargetLength CONSTANT)
	Q_PROPERTY(bool ignoreCase READ ignoreCase WRITE setIgnoreCase NOTIFY ignoreCaseChanged)
	Q_PROPERTY(bool autoPause READ autoPause WRITE setAutoPause NOTIFY autoPauseChanged)
	Q_PROPERTY(QString customText READ customText WRITE setCustomText NOTIFY customTextChanged)

	// ----- Цвета подсветки текста (задаются темой QML) -----
	Q_PROPERTY(QColor pendingColor READ pendingColor WRITE setPendingColor NOTIFY colorsChanged)
	Q_PROPERTY(QColor correctColor READ correctColor WRITE setCorrectColor NOTIFY colorsChanged)
	Q_PROPERTY(QColor wrongColor READ wrongColor WRITE setWrongColor NOTIFY colorsChanged)
	Q_PROPERTY(
	    QColor wrongBackground READ wrongBackground WRITE setWrongBackground NOTIFY colorsChanged)

	Q_PROPERTY(QString dataLocation READ dataLocation CONSTANT)
	Q_PROPERTY(QUrl dataLocationUrl READ dataLocationUrl CONSTANT)

public:
	/// \brief Статус сессии для QML (зеркало SessionStatus).
	/// \note Базовый тип - int: перечисления с другим размером QML до Qt 6.8 читает ненадёжно.
	enum class Status // NOLINT(performance-enum-size)
	{
		Inactive,
		Active,
		Paused,
		Completed
	};
	Q_ENUM(Status)

	explicit QmlTypingTrainerAdapter(QObject* parent = nullptr);
	~QmlTypingTrainerAdapter() override;

	QmlTypingTrainerAdapter(const QmlTypingTrainerAdapter&)            = delete;
	QmlTypingTrainerAdapter(QmlTypingTrainerAdapter&&)                 = delete;
	QmlTypingTrainerAdapter& operator=(const QmlTypingTrainerAdapter&) = delete;
	QmlTypingTrainerAdapter& operator=(QmlTypingTrainerAdapter&&)      = delete;

	/// \brief Начать сессию на новом тексте (Smart - генерация, иначе - свой текст).
	Q_INVOKABLE void start();
	/// \brief Начать заново на том же тексте.
	Q_INVOKABLE void restart();
	Q_INVOKABLE void stop();
	Q_INVOKABLE void pause();
	Q_INVOKABLE void resume();

	/// \brief Передать введённые символы (event.text нажатия).
	/// \note Управляющие символы отбрасываются: Enter, Tab и сочетания обрабатывает QML.
	Q_INVOKABLE void typeText(const QString& text);
	Q_INVOKABLE void backspace();
	/// \brief Перевод строки: засчитывается, только если текст ждёт его в этом месте.
	Q_INVOKABLE void enter();

	/// \brief Запросить свежую статистику (придёт через statisticsChanged).
	Q_INVOKABLE void requestStatistics();
	/// \brief Стереть статистику сочетаний и историю тренировок.
	Q_INVOKABLE void resetStatistics();

	[[nodiscard]] Status  status() const { return status_; }
	[[nodiscard]] QString formattedText() const { return formatted_text_; }
	[[nodiscard]] int     textLength() const { return static_cast<int>(chars_.size()); }
	[[nodiscard]] int     displayOffset() const { return static_cast<int>(page_start_); }
	[[nodiscard]] int     displayCursorPosition() const { return display_cursor_; }
	[[nodiscard]] int     cursorPosition() const { return cursor_position_; }
	[[nodiscard]] bool    typingStarted() const { return metrics_.keystrokes > 0; }

	[[nodiscard]] double wpm() const { return metrics_.wpm; }
	[[nodiscard]] double cpm() const { return metrics_.cpm; }
	[[nodiscard]] double accuracy() const { return metrics_.accuracy; }
	[[nodiscard]] double consistency() const { return metrics_.consistency; }
	[[nodiscard]] double elapsed() const { return metrics_.elapsed_seconds; }

	[[nodiscard]] QVariantMap  lastResult() const { return last_result_; }
	[[nodiscard]] QVariantList weakNgrams() const { return weak_ngrams_; }
	[[nodiscard]] QVariantList history() const { return history_; }

	[[nodiscard]] bool    smartMode() const { return smart_mode_; }
	[[nodiscard]] QString language() const;
	[[nodiscard]] double  difficulty() const { return difficulty_; }
	[[nodiscard]] int     targetLength() const { return target_length_; }
	[[nodiscard]] bool    ignoreCase() const { return ignore_case_; }
	[[nodiscard]] bool    autoPause() const { return auto_pause_; }
	[[nodiscard]] QString customText() const { return custom_text_; }

	[[nodiscard]] static int minTargetLength() { return K_MIN_TARGET_LENGTH; }
	[[nodiscard]] static int maxTargetLength() { return K_MAX_TARGET_LENGTH; }

	void setSmartMode(bool enabled);
	void setLanguage(const QString& code);
	void setDifficulty(double value);
	void setTargetLength(int length);
	void setIgnoreCase(bool enabled);
	void setAutoPause(bool enabled);
	void setCustomText(const QString& text);

	[[nodiscard]] QColor pendingColor() const { return pending_color_; }
	[[nodiscard]] QColor correctColor() const { return correct_color_; }
	[[nodiscard]] QColor wrongColor() const { return wrong_color_; }
	[[nodiscard]] QColor wrongBackground() const { return wrong_background_; }

	void setPendingColor(const QColor& color);
	void setCorrectColor(const QColor& color);
	void setWrongColor(const QColor& color);
	void setWrongBackground(const QColor& color);

	[[nodiscard]] QString dataLocation() const { return data_location_; }
	[[nodiscard]] QUrl    dataLocationUrl() const { return QUrl::fromLocalFile(data_location_); }

signals:
	void statusChanged();
	void formattedTextChanged();
	void cursorPositionChanged();
	void metricsChanged();
	void sessionFinished();
	void statisticsChanged();
	/// \brief Нажатие в раскладке не того алфавита; expectedLanguage - "ru" или "en".
	void layoutMismatch(const QString& expected_language);

	void smartModeChanged();
	void languageChanged();
	void difficultyChanged();
	void targetLengthChanged();
	void ignoreCaseChanged();
	void autoPauseChanged();
	void customTextChanged();
	void colorsChanged();

private:
	/// \brief Забрать все события ядра (вызывается в UI-потоке).
	Q_INVOKABLE void onOutputReady();

	void applyState(const SessionState& state);
	void applyUpdate(const StateUpdate& update);
	void applyResult(const SessionResult& result);
	void applyStatistics(const StatisticsSnapshot& snapshot);

	void setStatus(SessionStatus status);
	void setMetrics(const SessionMetrics& metrics);

	/// \brief Перестроить HTML-подсветку текущей страницы текста.
	void rebuildFormattedText();

	/// \brief Разбить текст на страницы (после замены chars_).
	void splitPages();

	/// \brief Перейти на страницу с курсором.
	void selectPage();

	/// \brief Текст для превью, пока сессия не запущена.
	void showPreview();

	void        loadSettings();
	static void saveSetting(const QString& key, const QVariant& value);
	void        saveCustomText() const;

	[[nodiscard]] SessionConfig sessionConfig() const;

	/// \brief Пределы длины текста умного режима, символов (они же - шкала в настройках).
	static constexpr int K_MIN_TARGET_LENGTH = 50;
	static constexpr int K_MAX_TARGET_LENGTH = 1000;

	std::unique_ptr<ITypingTrainerCore> core_;
	QString                             data_location_;
	std::filesystem::path               data_dir_;

	// Кэш состояния для QML
	Status                   status_ = Status::Inactive;
	std::vector<CharState>   chars_;
	int                      cursor_position_ = 0;
	int                      display_cursor_  = 0;
	std::vector<std::size_t> page_starts_{0}; ///< Начала страниц длинного текста.
	std::size_t              page_start_ = 0;
	std::size_t              page_end_   = 0;
	SessionMetrics           metrics_;
	QString                  formatted_text_;
	QVariantMap              last_result_;
	QVariantList             weak_ngrams_;
	QVariantList             history_;

	// Настройки
	bool    smart_mode_    = true;
	bool    russian_       = true;
	double  difficulty_    = 0.7;
	int     target_length_ = 250;
	bool    ignore_case_   = false;
	bool    auto_pause_    = true;
	QString custom_text_; ///< Всегда в виде для набора: пустой - набирать нечего.

	// Цвета подсветки
	QColor pending_color_{0x9E, 0x9E, 0x9E};
	QColor correct_color_{0x2B, 0x2B, 0x2B};
	QColor wrong_color_{0xE5, 0x39, 0x35};
	QColor wrong_background_{0xFD, 0xEC, 0xEA};
};

} // namespace typing_trainer
