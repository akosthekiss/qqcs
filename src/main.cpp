#include "camera/CameraManager.h"
#include "config/ConfigModel.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

#include <gst/gst.h>

namespace {

QString resolveConfigPath(const QStringList &args)
{
    for (int i = 0; i < args.size() - 1; ++i) {
        if (args.at(i) == QStringLiteral("--config"))
            return args.at(i + 1);
    }
    if (qEnvironmentVariableIsSet("QQCS_CONFIG"))
        return qEnvironmentVariable("QQCS_CONFIG");
    return ConfigModel::resolveDefaultPath();
}

} // namespace

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    QGuiApplication app(argc, argv);

    ConfigModel configModel;
    const QString configPath = resolveConfigPath(app.arguments());
    if (!configModel.load(configPath)) {
        qWarning() << "Config validation failed for" << configPath << ":";
        for (const auto &error : configModel.errors())
            qWarning().noquote() << " -" << error;
    }

    CameraManager cameraManager(configModel.appConfig());
    cameraManager.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("configModel"), &configModel);
    engine.rootContext()->setContextProperty(QStringLiteral("cameraManager"), &cameraManager);
    engine.rootContext()->setContextProperty(QStringLiteral("cameraListModel"), cameraManager.listModel());
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("QqcsUi", "Main");

    // Manual dev/debug aid: dump a frame of the (possibly occluded/off-screen)
    // main window without depending on OS-level screen capture permissions.
    if (const char *shotPath = std::getenv("QQCS_DEBUG_SCREENSHOT")) {
        if (!engine.rootObjects().isEmpty()) {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
                QTimer::singleShot(2000, window, [window, shotPath]() {
                    window->grabWindow().save(QString::fromLocal8Bit(shotPath));
                    qInfo() << "[debug] saved screenshot to" << shotPath;
                });
            }
        }
    }

    return app.exec();
}
