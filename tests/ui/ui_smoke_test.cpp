// Дымовые тесты интерфейса: настоящий Main.qml без экрана (QT_QPA_PLATFORM=offscreen),
// ввод через эмуляцию клавиатуры. Если задана переменная TT_SCREENSHOT_DIR, по ходу
// сценариев сохраняются скриншоты (так же обновляются картинки для README).

#include "typing_trainer_adapter.hpp"

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

#include <algorithm>

Q_IMPORT_QML_PLUGIN(TypingTrainerModulePlugin)

using typing_trainer::QmlTypingTrainerAdapter;
using Status = QmlTypingTrainerAdapter::Status;

class UiSmokeTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void cleanupTestCase();

	void startsSmartSessionOnLaunch();
	void typingMarksCharsAndUpdatesMetrics();
	void escapePausesAndTypingResumes();
	void wrongLayoutIsReported();
	void freeTextSessionCompletesWithResult();
	void statisticsPageShowsHistory();
	void settingsPageOpensAndCloses();
	void themesRenderWithoutWarnings();
	void autoPauseStopsTheClock();
	void longTextIsShownInPages();

private:
	/// \brief Текст текущей сессии без HTML-разметки.
	[[nodiscard]] QString currentText() const;

	/// \brief Дождаться активной сессии с непустым текстом.
	void waitForActiveSession() const;

	/// \brief Напечатать строку с паузой между нажатиями.
	void type(const QString& text, int delay_ms = 30) const;

	/// \brief Пауза между нажатиями «как у человека»: для скриншотов ~55 WPM, иначе быстрее.
	[[nodiscard]] static int humanDelay();

	/// \brief Сохранить скриншот, если задан TT_SCREENSHOT_DIR.
	void snapshot(const QString& name) const;

	void setTheme(const QString& theme) const;

	/// \brief Демо-история и статистика для скриншотов (только при TT_SCREENSHOT_DIR).
	void seedDemoData() const;

	/// \brief Нажать кнопку с заданной надписью.
	void clickButton(const QString& text) const;

	QTemporaryDir            data_dir_;
	QTemporaryDir            settings_dir_;
	QQmlApplicationEngine*   engine_  = nullptr;
	QQuickWindow*            window_  = nullptr;
	QmlTypingTrainerAdapter* trainer_ = nullptr;
	QStringList              warnings_;
};

void UiSmokeTest::initTestCase()
{
	// Тесты не должны трогать настоящие данные и настройки пользователя.
	QCoreApplication::setOrganizationName("TypingTrainerTests");
	QCoreApplication::setApplicationName("UiSmokeTest");
	QVERIFY(data_dir_.isValid());
	QVERIFY(settings_dir_.isValid());
	qputenv("TYPING_TRAINER_DATA_DIR", data_dir_.path().toUtf8());
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir_.path());
	if (!qEnvironmentVariable("TT_SCREENSHOT_DIR").isEmpty()) seedDemoData();

	QQuickStyle::setStyle("Material");
	QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);

	engine_ = new QQmlApplicationEngine(this);
	connect(engine_, &QQmlEngine::warnings, this, [this](const QList<QQmlError>& errors) {
		for (auto const& error : errors)
			warnings_.append(error.toString());
	});
	engine_->loadFromModule("TypingTrainerModule", "Main");

	QCOMPARE(engine_->rootObjects().size(), 1);
	window_ = qobject_cast<QQuickWindow*>(engine_->rootObjects().constFirst());
	QVERIFY(window_ != nullptr);
	window_->resize(1100, 720);
	window_->show();
	window_->requestActivate();
	QVERIFY(QTest::qWaitForWindowExposed(window_));

	trainer_
	    = engine_->singletonInstance<QmlTypingTrainerAdapter*>("TypingTrainerModule", "Trainer");
	QVERIFY(trainer_ != nullptr);
	// Автопауза проверяется отдельным тестом, остальным она мешала бы.
	trainer_->setAutoPause(false);
}

void UiSmokeTest::cleanupTestCase()
{
	QVERIFY2(warnings_.isEmpty(), qPrintable(warnings_.join('\n')));
	delete engine_; // ядро сохраняет данные при уничтожении
	engine_ = nullptr;
	QVERIFY(QFile::exists(QDir(data_dir_.path()).filePath("ngram_stats.json")));
}

QString UiSmokeTest::currentText() const
{
	QTextDocument document;
	document.setHtml(trainer_->formattedText());
	return document.toPlainText();
}

void UiSmokeTest::waitForActiveSession() const
{
	QTRY_COMPARE(trainer_->status(), Status::Active);
	QTRY_VERIFY(trainer_->textLength() > 0);
	QTRY_COMPARE(currentText().size(), trainer_->textLength());
}

int UiSmokeTest::humanDelay()
{ return qEnvironmentVariable("TT_SCREENSHOT_DIR").isEmpty() ? 45 : 210; }

void UiSmokeTest::type(const QString& text, int delay_ms) const
{
	// Для QWindow в QtTest нет keyClicks: отправляем нажатие с текстом на каждый символ.
	for (QChar const ch : text)
	{
		QTest::qWait(delay_ms);
		QTest::sendKeyEvent(QTest::Click, window_, Qt::Key_unknown, QString(ch), Qt::NoModifier);
	}
}

void UiSmokeTest::snapshot(const QString& name) const
{
	QString const dir = qEnvironmentVariable("TT_SCREENSHOT_DIR");
	if (dir.isEmpty()) return;
	QDir().mkpath(dir);
	QTest::qWait(250); // даём доиграть анимациям курсора и прокрутки
	QVERIFY(window_->grabWindow().save(QDir(dir).filePath(name + ".png")));
}

void UiSmokeTest::setTheme(const QString& theme) const
{
	auto* theme_object = engine_->singletonInstance<QObject*>("TypingTrainerModule", "Theme");
	QVERIFY(theme_object != nullptr);
	theme_object->setProperty("currentTheme", theme);
}

void UiSmokeTest::seedDemoData() const
{
	// Месяц тренировок с постепенным ростом скорости.
	QJsonArray sessions;
	QDateTime  finished = QDateTime::currentDateTime().addDays(-30);
	for (int i = 0; i < 28; ++i)
	{
		double const wpm = 36.0 + (i * 0.9) + ((i * 7) % 5) - 2.0;
		sessions.append(QJsonObject{
		    {"finished_at", finished.toSecsSinceEpoch()},
		    {"mode", i % 4 == 3 ? "free" : "smart"},
		    {"language", "ru"},
		    {"length", 250},
		    {"errors", 3 + (i % 4)},
		    {"wpm", wpm},
		    {"cpm", wpm * 5.0},
		    {"accuracy", 94.0 + ((i % 5) * 0.8)},
		    {"consistency", 62.0 + ((i % 6) * 2.0)},
		    {"duration", 250.0 / (wpm * 5.0) * 60.0},
		    {"keystrokes", 258},
		});
		finished = finished.addSecs(86'400 + ((i % 3) * 3'600));
	}

	// Слабые места: редкие буквы и неудобные переходы русской раскладки.
	QJsonArray ngrams;
	auto const add_ngram
	    = [&ngrams](const QString& gram, double avg_time, double error_rate, int attempts) {
		      QJsonArray code_points;
		      for (char32_t const ch : gram.toStdU32String())
			      code_points.append(static_cast<qint64>(ch));
		      double const weight = std::min(attempts, 50);
		      ngrams.append(QJsonObject{{"gram", code_points},
			                            {"attempts", attempts},
			                            {"attempt_weight", weight},
			                            {"error_weight", weight * error_rate},
			                            {"time_weight", weight},
			                            {"avg_time", avg_time}});
	      };
	QString const common = QStringLiteral("оеаинтсрвлкмдпуяыьгзб");
	for (QChar const ch : common)
		add_ngram(QString(ch), 0.17, 0.02, 400);
	add_ngram(QStringLiteral("щ"), 0.41, 0.14, 60);
	add_ngram(QStringLiteral("ъ"), 0.46, 0.18, 24);
	add_ngram(QStringLiteral("ю"), 0.33, 0.09, 120);
	add_ngram(QStringLiteral("ж"), 0.30, 0.07, 150);
	add_ngram(QStringLiteral("ц"), 0.31, 0.08, 90);
	add_ngram(QStringLiteral("ф"), 0.29, 0.06, 70);
	add_ngram(QStringLiteral("э"), 0.28, 0.05, 80);
	add_ngram(QStringLiteral("ё"), 0.38, 0.11, 40);
	add_ngram(QStringLiteral("ых"), 0.27, 0.06, 110);
	add_ngram(QStringLiteral("шь"), 0.26, 0.05, 95);
	add_ngram(QStringLiteral("вз"), 0.29, 0.04, 45);
	add_ngram(QStringLiteral("ств"), 0.25, 0.05, 130);

	auto const write = [this](const QString& name, const QJsonObject& root) {
		QFile file(QDir(data_dir_.path()).filePath(name));
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(QJsonDocument(root).toJson());
	};
	write(QStringLiteral("history.json"), QJsonObject{{"version", 1}, {"sessions", sessions}});
	write(QStringLiteral("ngram_stats.json"), QJsonObject{{"version", 3}, {"ngrams", ngrams}});
}

void UiSmokeTest::startsSmartSessionOnLaunch()
{
	QVERIFY(trainer_->smartMode());
	waitForActiveSession();
	QVERIFY(!trainer_->typingStarted());
	QCOMPARE(trainer_->cursorPosition(), 0);
	snapshot("01-ready");
}

void UiSmokeTest::typingMarksCharsAndUpdatesMetrics()
{
	waitForActiveSession();
	QString const text = currentText();

	type(text.left(40), humanDelay());
	QTRY_COMPARE(trainer_->cursorPosition(), 40);
	QVERIFY(trainer_->typingStarted());
	QTRY_VERIFY(trainer_->wpm() > 0.0);
	QCOMPARE(trainer_->accuracy(), 100.0);

	// Ошибка: вместо следующего символа - цифра.
	type(QStringLiteral("7"));
	QTRY_COMPARE(trainer_->cursorPosition(), 41);
	QTRY_VERIFY(trainer_->accuracy() < 100.0);
	type(text.mid(41, 14), humanDelay());
	QTRY_COMPARE(trainer_->cursorPosition(), 55);
	snapshot("02-typing-light");

	QTest::keyClick(window_, Qt::Key_Backspace);
	QTRY_COMPARE(trainer_->cursorPosition(), 54);
}

void UiSmokeTest::escapePausesAndTypingResumes()
{
	waitForActiveSession();

	QTest::keyClick(window_, Qt::Key_Escape);
	QTRY_COMPARE(trainer_->status(), Status::Paused);
	snapshot("03-paused");

	int const position = trainer_->cursorPosition();
	type(currentText().mid(position, 1));
	QTRY_COMPARE(trainer_->status(), Status::Active);
	QTRY_COMPARE(trainer_->cursorPosition(), position + 1);
}

void UiSmokeTest::wrongLayoutIsReported()
{
	trainer_->setLanguage(QStringLiteral("en"));
	trainer_->start();
	QTRY_VERIFY(trainer_->status() == Status::Active && !trainer_->typingStarted()
	            && currentText().front().isLower() && currentText().front().unicode() < 0x80);

	QSignalSpy const spy(trainer_, &QmlTypingTrainerAdapter::layoutMismatch);
	type(QStringLiteral("ф"));
	QTRY_COMPARE(spy.count(), 1);
	QCOMPARE(spy.constFirst().constFirst().toString(), QStringLiteral("en"));
	QCOMPARE(trainer_->cursorPosition(), 0); // нажатие не засчитано
	snapshot("04-layout-warning");
}

void UiSmokeTest::freeTextSessionCompletesWithResult()
{
	trainer_->setCustomText(QStringLiteral("Быстрая «лиса» — прыгает…"));
	QCOMPARE(trainer_->customText(), QStringLiteral("Быстрая \"лиса\" - прыгает..."));

	QSignalSpy const finished(trainer_, &QmlTypingTrainerAdapter::sessionFinished);
	trainer_->setSmartMode(false);
	trainer_->start();
	QTRY_VERIFY(trainer_->status() == Status::Active && currentText() == trainer_->customText());

	type(currentText(), humanDelay());
	QTRY_COMPARE(trainer_->status(), Status::Completed);
	QTRY_COMPARE(finished.count(), 1);

	QVariantMap const result = trainer_->lastResult();
	QVERIFY(result.value("wpm").toDouble() > 0.0);
	QCOMPARE(result.value("mode").toString(), QStringLiteral("free"));
	QCOMPARE(result.value("language").toString(), QStringLiteral("ru"));
	snapshot("05-result");

	// Enter после завершения - следующая тренировка.
	QTest::keyClick(window_, Qt::Key_Return);
	QTRY_COMPARE(trainer_->status(), Status::Active);

	trainer_->setSmartMode(true);
	trainer_->start();
	QTRY_VERIFY(trainer_->status() == Status::Active && currentText() != trainer_->customText());
}

void UiSmokeTest::clickButton(const QString& text) const
{
	for (auto* item : window_->findChildren<QQuickItem*>())
	{
		if (item->isVisible() && item->property("text").toString() == text
		    && item->metaObject()->indexOfSignal("clicked()") >= 0)
		{
			QVERIFY(QMetaObject::invokeMethod(item, "clicked"));
			return;
		}
	}
	QFAIL(qPrintable("нет кнопки " + text));
}

void UiSmokeTest::statisticsPageShowsHistory()
{
	// К этому моменту одна тренировка (свободный текст) уже завершена.
	clickButton(QStringLiteral("Статистика"));
	QTRY_VERIFY(window_->findChild<QQuickItem*>("statisticsPage") != nullptr);
	QTRY_VERIFY(!trainer_->history().isEmpty());
	QVERIFY(!trainer_->weakNgrams().isEmpty());
	snapshot("08-statistics");

	QTest::keyClick(window_, Qt::Key_Escape);
	QTRY_VERIFY(window_->findChild<QQuickItem*>("statisticsPage") == nullptr);
}

void UiSmokeTest::settingsPageOpensAndCloses()
{
	clickButton(QStringLiteral("Настройки"));
	QTRY_VERIFY(window_->findChild<QQuickItem*>("settingsPage") != nullptr);
	snapshot("06-settings");

	QTest::keyClick(window_, Qt::Key_Escape);
	QTRY_VERIFY(window_->findChild<QQuickItem*>("settingsPage") == nullptr);
}

void UiSmokeTest::themesRenderWithoutWarnings()
{
	waitForActiveSession();
	type(currentText().left(24), humanDelay());
	type(QStringLiteral("9"));
	QTRY_COMPARE(trainer_->cursorPosition(), 25);

	for (QString const& theme :
	     {QStringLiteral("dark"), QStringLiteral("black"), QStringLiteral("light")})
	{
		setTheme(theme);
		snapshot("07-theme-" + theme);
	}
}

void UiSmokeTest::autoPauseStopsTheClock()
{
	trainer_->setAutoPause(true);
	trainer_->start();
	QTRY_VERIFY(trainer_->status() == Status::Active && !trainer_->typingStarted());

	// Пока ничего не набрано, автопауза не нужна: время ещё не идёт.
	QTest::qWait(5500);
	QCOMPARE(trainer_->status(), Status::Active);

	type(currentText().left(5));
	QTRY_VERIFY_WITH_TIMEOUT(trainer_->status() == Status::Paused, 8000);
	trainer_->setAutoPause(false);
}

void UiSmokeTest::longTextIsShownInPages()
{
	QString text;
	while (text.size() < 5000)
		text += QStringLiteral("длинный текст для проверки страниц ");
	trainer_->setCustomText(text);
	trainer_->setSmartMode(false);
	trainer_->start();
	QTRY_VERIFY(trainer_->status() == Status::Active
	            && trainer_->textLength() == text.trimmed().size());
	QCOMPARE(trainer_->displayOffset(), 0);
	QVERIFY(currentText().size() < 2100); // показана только первая страница

	QString const first_page = currentText();
	type(first_page, 0);
	QTRY_COMPARE(trainer_->cursorPosition(), first_page.size());
	QTRY_COMPARE(trainer_->displayOffset(), first_page.size()); // курсор перешёл на вторую страницу
	QVERIFY(first_page.endsWith(' '));                          // граница проходит между словами
	QVERIFY(currentText().front().isLetter());

	QTest::keyClick(window_, Qt::Key_Backspace); // назад через границу страниц
	QTRY_COMPARE(trainer_->displayOffset(), 0);

	trainer_->setSmartMode(true);
	trainer_->start();
	QTRY_VERIFY(trainer_->status() == Status::Active && trainer_->displayOffset() == 0);
}

QTEST_MAIN(UiSmokeTest)
#include "ui_smoke_test.moc"
