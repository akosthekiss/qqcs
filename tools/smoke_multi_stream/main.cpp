// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

// Manual verification tool for concurrent CameraManager pipelines (mosaic
// sub-stream + lazily-started fullscreen main-stream running at once).
// No ctest integration -- requires a live RTSP camera.
//
//   QQCS_SMOKE_MAIN_URL="rtsp://user:pass@host/main" \
//   QQCS_SMOKE_SUB_URL="rtsp://user:pass@host/sub" \
//   ./smoke_multi_stream

#include "camera/AppSinkVideoItem.h"
#include "camera/CameraManager.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

#include <gst/gst.h>

#include <cstdlib>

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    QGuiApplication app(argc, argv);

    const char *mainUrl = std::getenv("QQCS_SMOKE_MAIN_URL");
    const char *subUrl = std::getenv("QQCS_SMOKE_SUB_URL");
    if (!mainUrl || !subUrl) {
        qWarning() << "Set QQCS_SMOKE_MAIN_URL and QQCS_SMOKE_SUB_URL before running this tool.";
        return 1;
    }

    AppConfig config;
    CameraConfig cam;
    cam.id = QStringLiteral("cam1");
    cam.mainUrl = QString::fromLocal8Bit(mainUrl);
    cam.subUrl = QString::fromLocal8Bit(subUrl);
    config.cameras = { cam };

    CameraManager manager(std::move(config));
    QObject::connect(manager.listModel(), &CameraListModel::dataChanged, [&manager]() {
        const auto state = manager.listModel()->data(manager.listModel()->index(0), CameraListModel::StateRole).toInt();
        qInfo() << "[mosaic pipeline] state ->" << state;
    });

    QQuickWindow window;
    window.resize(1280, 480);
    window.setColor(Qt::black);

    manager.start(); // mosaic (sub) pipeline starts running continuously
    manager.enterFullscreen(QStringLiteral("cam1")); // fullscreen (main) pipeline starts concurrently

    AppSinkVideoItem *mosaicItem = manager.mosaicVideoItem(QStringLiteral("cam1"));
    mosaicItem->setParentItem(window.contentItem());
    mosaicItem->setPosition(QPointF(0, 0));
    mosaicItem->setSize(QSizeF(640, 480));

    AppSinkVideoItem *fullscreenItem = manager.fullscreenVideoItem();
    fullscreenItem->setParentItem(window.contentItem());
    fullscreenItem->setPosition(QPointF(640, 0));
    fullscreenItem->setSize(QSizeF(640, 480));

    QTimer heartbeat;
    int ticks = 0;
    QObject::connect(&heartbeat, &QTimer::timeout, [&ticks]() {
        qInfo() << "[main-thread heartbeat]" << ++ticks;
    });
    heartbeat.start(500);

    window.show();

    if (const char *shotPath = std::getenv("QQCS_SMOKE_SCREENSHOT")) {
        QTimer::singleShot(4000, &window, [&window, shotPath]() {
            window.grabWindow().save(QString::fromLocal8Bit(shotPath));
            qInfo() << "[smoke] saved screenshot to" << shotPath;
        });
    }

    QTimer::singleShot(8000, &app, &QGuiApplication::quit);
    return app.exec();
}
