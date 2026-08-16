#include "camera/CameraManager.h"
#include "config/ConfigModel.h"
#include "navigation/NavigationController.h"

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

    NavigationController navigationController(cameraManager.cameraIds(), configModel.appConfig().layout.columns);
    navigationController.setShortcutMap(cameraManager.shortcutDigitToIndex());

    // NavigationController owns view/zoom/pan state; CameraManager owns
    // pipeline lifecycle. Connected here (before the QML engine loads) so
    // pipeline switches always happen before any QML Connections reacting
    // to the same signal (Qt invokes same-signal slots in connection order).
    QObject::connect(&navigationController, &NavigationController::fullscreenCameraActivated, &cameraManager,
                      &CameraManager::switchFullscreenCamera);
    QObject::connect(&navigationController, &NavigationController::fullscreenExited, &cameraManager,
                      &CameraManager::exitFullscreen);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("configModel"), &configModel);
    engine.rootContext()->setContextProperty(QStringLiteral("cameraManager"), &cameraManager);
    engine.rootContext()->setContextProperty(QStringLiteral("cameraListModel"), cameraManager.listModel());
    engine.rootContext()->setContextProperty(QStringLiteral("navigationController"), &navigationController);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("QqcsUi", "Main");

    // Manual dev/debug aid: simulate a mosaic tile click without needing
    // real GUI automation, to verify the click -> fullscreen wiring.
    if (const char *tileIndex = std::getenv("QQCS_DEBUG_CLICK_TILE")) {
        const int index = QByteArray(tileIndex).toInt();
        QTimer::singleShot(1000, &navigationController,
                            [&navigationController, index] { navigationController.selectMosaicTile(index); });
    }

    // Manual dev/debug aid: simulate N wheel-zoom-in steps at the viewport
    // center, to verify zoom rendering without synthesizing real GUI wheel
    // events through the window system.
    if (const char *zoomSteps = std::getenv("QQCS_DEBUG_ZOOM_STEPS")) {
        const int steps = QByteArray(zoomSteps).toInt();
        QTimer::singleShot(1500, &navigationController, [&navigationController, steps] {
            for (int i = 0; i < steps; ++i)
                navigationController.handleWheelZoom(1.0, QPointF(640, 360));
        });
    }

    // Manual dev/debug aid: dump a frame of the (possibly occluded/off-screen)
    // main window without depending on OS-level screen capture permissions.
    if (const char *shotPath = std::getenv("QQCS_DEBUG_SCREENSHOT")) {
        if (!engine.rootObjects().isEmpty()) {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
                QTimer::singleShot(4000, window, [window, shotPath]() {
                    window->grabWindow().save(QString::fromLocal8Bit(shotPath));
                    qInfo() << "[debug] saved screenshot to" << shotPath;
                });
            }
        }
    }

    // Manual dev/debug aid: quit naturally after N ms instead of being
    // killed, so buffered stdout/stderr actually flushes (SIGTERM does not
    // trigger Qt's normal shutdown flush).
    if (const char *quitAfterMs = std::getenv("QQCS_DEBUG_QUIT_AFTER_MS"))
        QTimer::singleShot(QByteArray(quitAfterMs).toInt(), &app, &QGuiApplication::quit);

    return app.exec();
}
