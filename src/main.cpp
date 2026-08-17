// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "camera/CameraManager.h"
#include "cec/CecAdapter.h"
#include "config/ConfigModel.h"
#include "input/InputManager.h"
#include "navigation/NavigationController.h"

#include <QGuiApplication>
#include <QKeyEvent>
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

    InputManager inputManager;
    QObject::connect(&inputManager, &InputManager::actionTriggered, &navigationController,
                      &NavigationController::handleInputAction);

    // SPEC §25/§28: CEC's absence (no library linked, or no adapter found)
    // must never block startup -- start()'s return is only ever logged.
    CecAdapter cecAdapter;
    QObject::connect(&cecAdapter, &CecAdapter::buttonPressed, &inputManager, &InputManager::handleCecButtonPress);
    if (!cecAdapter.start())
        qInfo() << "Running without HDMI-CEC.";

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
    engine.rootContext()->setContextProperty(QStringLiteral("inputManager"), &inputManager);
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
        QPointF pivot(640, 360); // default: viewport center
        if (const char *cursor = std::getenv("QQCS_DEBUG_ZOOM_CURSOR")) {
            const QByteArrayList parts = QByteArray(cursor).split(',');
            if (parts.size() == 2)
                pivot = QPointF(parts.at(0).toDouble(), parts.at(1).toDouble());
        }
        QTimer::singleShot(1500, &navigationController, [&navigationController, steps, pivot] {
            for (int i = 0; i < steps; ++i)
                navigationController.handleWheelZoom(1.0, pivot);
        });
    }

    // Manual dev/debug aid: simulate a pan drag delta (comma-separated
    // "dx,dy"), to verify pan rendering/clamping without synthesizing real
    // GUI drag events through the window system.
    if (const char *panDelta = std::getenv("QQCS_DEBUG_PAN_DELTA")) {
        const QByteArrayList parts = QByteArray(panDelta).split(',');
        if (parts.size() == 2) {
            const qreal dx = parts.at(0).toDouble();
            const qreal dy = parts.at(1).toDouble();
            QTimer::singleShot(2500, &navigationController,
                                [&navigationController, dx, dy] { navigationController.handlePanDragDelta(QPointF(dx, dy)); });
        }
    }

    // Manual dev/debug aid: inject a real QKeyEvent into the window (same
    // Qt Quick focus-item delivery path OS input would use), to verify the
    // QML Keys.onPressed -> inputManager.handleKeyEvent wiring without
    // needing OS-level Accessibility permissions for synthetic keystrokes.
    if (const char *injectKey = std::getenv("QQCS_DEBUG_INJECT_KEY")) {
        // Comma-separated list of Qt::Key codes, sent in order. sendEvent()
        // is synchronous -- each key's full handleInputAction() call (and
        // any resulting state change) completes before the next is sent,
        // so no inter-key delay is needed.
        QVector<int> keys;
        for (const QByteArray &part : QByteArray(injectKey).split(','))
            keys.append(part.toInt());
        // 2500ms: deliberately after QQCS_DEBUG_ZOOM_STEPS/CLICK_TILE's
        // delays, so this hook can verify state-dependent behavior (e.g.
        // Escape resetting a zoom applied by the other debug hooks first).
        QTimer::singleShot(2500, &engine, [&engine, keys] {
            if (engine.rootObjects().isEmpty())
                return;
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
                for (int key : keys) {
                    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
                    QCoreApplication::sendEvent(window, &press);
                    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
                    QCoreApplication::sendEvent(window, &release);
                }
            }
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
