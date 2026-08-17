// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

// Manual verification tool for RtspStreamPipeline (no ctest integration --
// requires a live RTSP camera). Reads the URL from QQCS_SMOKE_URL so no
// credential ever needs to be written into the repository.
//
//   QQCS_SMOKE_URL="rtsp://user:pass@host/path" ./smoke_single_stream

#include "camera/AppSinkVideoItem.h"
#include "camera/RtspStreamPipeline.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>

#include <gst/gst.h>

#include <cstdlib>

namespace {

const char *stateName(CameraState state)
{
    switch (state) {
    case CameraState::Disconnected:
        return "Disconnected";
    case CameraState::Connecting:
        return "Connecting";
    case CameraState::Live:
        return "Live";
    case CameraState::Lost:
        return "Lost";
    }
    return "?";
}

} // namespace

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    QGuiApplication app(argc, argv);

    const char *url = std::getenv("QQCS_SMOKE_URL");
    if (!url) {
        qWarning() << "Set QQCS_SMOKE_URL to an RTSP url before running this tool.";
        return 1;
    }

    RtspStreamPipeline pipeline(QString::fromLocal8Bit(url));
    QObject::connect(&pipeline, &RtspStreamPipeline::stateChanged, [](CameraState state) {
        qInfo() << "[pipeline] state ->" << stateName(state);
    });

    QQuickWindow window;
    window.resize(960, 540);
    window.setColor(Qt::black);

    AppSinkVideoItem *videoItem = pipeline.videoItem();
    videoItem->setParentItem(window.contentItem());
    videoItem->setSize(QSizeF(960, 540));

    QTimer heartbeat;
    int ticks = 0;
    QObject::connect(&heartbeat, &QTimer::timeout, [&ticks]() {
        qInfo() << "[main-thread heartbeat]" << ++ticks;
    });
    heartbeat.start(500);

    window.show();
    pipeline.start();

    if (const char *shotPath = std::getenv("QQCS_SMOKE_SCREENSHOT")) {
        QTimer::singleShot(4000, &window, [&window, shotPath]() {
            window.grabWindow().save(QString::fromLocal8Bit(shotPath));
            qInfo() << "[smoke] saved screenshot to" << shotPath;
        });
    }

    QTimer::singleShot(8000, &app, &QGuiApplication::quit);
    return app.exec();
}
