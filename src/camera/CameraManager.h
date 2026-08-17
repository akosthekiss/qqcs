// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include "CameraListModel.h"
#include "RtspStreamPipeline.h"
#include "config/AppConfig.h"

#include <QHash>
#include <QObject>
#include <QQuickItem>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <memory>

// Owns camera pipeline lifecycle. Mosaic (sub-or-main, SPEC §32) pipelines
// are created for every camera and run continuously once start() is called
// -- mosaic always shows every camera at once, so there is no lazy-start
// benefit there. The fullscreen (always main) pipeline is created lazily,
// one at a time, only for whichever camera is currently fullscreen: at most
// one full-resolution decode ever runs concurrently, which is the
// structural answer to SPEC §30's "no arbitrary camera-count cap".
class CameraManager : public QObject
{
    Q_OBJECT

public:
    explicit CameraManager(AppConfig config, QObject *parent = nullptr);

    CameraListModel *listModel() const { return m_listModel; }
    QString currentFullscreenId() const { return m_fullscreenId; }
    Q_INVOKABLE QString firstCameraId() const { return m_runtimes.isEmpty() ? QString() : m_runtimes.first().config.id; }

    QStringList cameraIds() const;
    // digit (0-9) -> index in cameraIds(), for NavigationController::setShortcutMap.
    QHash<int, int> shortcutDigitToIndex() const;

    // The fullscreen (main-stream) pipeline is a different stream than its
    // camera's mosaic tile, so it has its own independent CameraState/
    // reconnect status -- this is deliberately separate from
    // CameraListModel, which only ever reflects mosaic pipelines.
    Q_INVOKABLE QVariantMap fullscreenStatus() const;

    // SPEC §22, fullscreen-only (same rationale as audio status).
    Q_INVOKABLE QVariantMap fullscreenDiagnostics() const;

    // Starts every mosaic pipeline. Deliberately not done in the
    // constructor, so constructing a CameraManager in tests never triggers
    // real network I/O; only main.cpp (and the multi-stream smoke tool)
    // call this.
    Q_INVOKABLE void start();

    AppSinkVideoItem *mosaicVideoItem(const QString &id) const;
    AppSinkVideoItem *fullscreenVideoItem() const;

    // Reparents the given camera's video item into `container` and anchors
    // it to fill that container. Called from QML (Component.onCompleted on
    // a plain placeholder Item) so QML never needs to know AppSinkVideoItem
    // exists as a type -- it only ever manipulates its own Item.
    Q_INVOKABLE void attachMosaicVideo(const QString &id, QQuickItem *container);
    Q_INVOKABLE void attachFullscreenVideo(QQuickItem *container);

    Q_INVOKABLE void focus(const QString &id);
    Q_INVOKABLE void enterFullscreen(const QString &id);
    Q_INVOKABLE void exitFullscreen();
    Q_INVOKABLE void switchFullscreenCamera(const QString &id);

signals:
    void fullscreenIdChanged(const QString &id);
    void fullscreenStatusChanged();

private:
    struct CameraRuntime {
        CameraConfig config;
        RtspStreamPipeline *mosaicPipeline = nullptr;
    };

    CameraRuntime *runtimeForId(const QString &id);
    void teardownFullscreenPipeline();

    AppConfig m_config;
    CameraListModel *m_listModel;
    QVector<CameraRuntime> m_runtimes;
    QString m_fullscreenId;
    std::unique_ptr<RtspStreamPipeline> m_fullscreenPipeline;
    bool m_started = false;
};
