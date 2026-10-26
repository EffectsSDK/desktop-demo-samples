#include "controller_settings.h"
#include "sample_controller.h"

#ifdef _WIN32
#include "camera/camera_capturer.h"
#endif

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>

namespace {

void setIfUnset(const char* name, const QByteArray& value)
{
	if (!qEnvironmentVariableIsSet(name)) {
		qputenv(name, value);
	}
}

}

int main(int argc, char* argv[])
{
#ifdef _WIN32
	const camera::MediaFoundation mediaFoundation;
#endif

	setIfUnset("QSG_RHI_DISABLE_DISK_CACHE", "1");
	setIfUnset("QML_DISABLE_DISK_CACHE", "1");

	QGuiApplication app(argc, argv);
	app.setOrganizationName("Damiko Inc.");
	app.setApplicationName("Video Effects SDK");
	app.setApplicationVersion(SAMPLE_VERSION);

	QCommandLineParser parser;
	parser.addHelpOption();
	QCommandLineOption screenshotOption(
		"screenshot",
		"Save the window to <file> once it has been drawn, then quit.",
		"file"
	);
	parser.addOption(screenshotOption);
	parser.process(app);

	QQuickStyle::setStyle("Material");

	SampleController controller;

	ControllerSettings settings;
	settings.load(controller);
	settings.watch(&controller);

	QQmlApplicationEngine engine;
	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed,
		&app, [] { QCoreApplication::exit(-1); },
		Qt::QueuedConnection
	);
	engine.setInitialProperties({
		{ "controller", QVariant::fromValue(&controller) },
		{ "copyright", QStringLiteral(SAMPLE_COPYRIGHT) }
	});
	engine.loadFromModule("SampleQml", "Main");

	controller.start();

	if (parser.isSet(screenshotOption)) {
		const auto rootObjects = engine.rootObjects();
		auto window = rootObjects.isEmpty()
			? nullptr
			: qobject_cast<QQuickWindow*>(rootObjects.first());
		if (nullptr == window) {
			return -1;
		}

		const QString path = parser.value(screenshotOption);
		QObject::connect(
			window, &QQuickWindow::frameSwapped,
			window, [window, path] {
				const bool saved = window->grabWindow().save(path);
				QCoreApplication::exit(saved ? 0 : 1);
			},
			Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection)
		);
	}

	return app.exec();
}
