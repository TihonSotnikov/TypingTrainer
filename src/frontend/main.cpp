#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdlib>

// Интерфейс собран статическим QML-модулем (см. src/frontend/CMakeLists.txt).
Q_IMPORT_QML_PLUGIN(TypingTrainerModulePlugin)

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);
	QGuiApplication::setOrganizationName("TihonSotnikov");
	QGuiApplication::setApplicationName("TypingTrainer");
	QGuiApplication::setApplicationDisplayName("Typing Trainer");
	QGuiApplication::setApplicationVersion(TT_VERSION);
	QGuiApplication::setWindowIcon(QIcon(":/qt/qml/TypingTrainerModule/icons/app.png"));

	QCommandLineParser parser;
	parser.setApplicationDescription("Тренажёр слепой печати с адаптивными упражнениями");
	parser.addHelpOption();
	parser.addVersionOption();
	QCommandLineOption smoke_test("smoke-test",
	                              "Загрузить интерфейс и выйти: проверка собранного дистрибутива.");
	smoke_test.setFlags(QCommandLineOption::HiddenFromHelp);
	parser.addOption(smoke_test);
	parser.process(app);

	// Единый стиль на всех платформах; цвета задаёт тема приложения (Theme.qml).
	QQuickStyle::setStyle("Material");

	QQmlApplicationEngine engine;
	QObject::connect(
	    &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
	    [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

	if (parser.isSet(smoke_test))
	{
		// Любое предупреждение QML здесь - признак битой сборки (например, не развёрнут модуль Qt).
		QObject::connect(&engine, &QQmlEngine::warnings, &app,
		                 [](const QList<QQmlError>& warnings) {
			                 for (auto const& warning : warnings)
				                 qCritical().noquote() << warning.toString();
			                 QCoreApplication::exit(EXIT_FAILURE);
		                 });
		QTimer::singleShot(1500, &app, [] { QCoreApplication::exit(EXIT_SUCCESS); });
	}

	engine.loadFromModule("TypingTrainerModule", "Main");

	return QGuiApplication::exec();
}
