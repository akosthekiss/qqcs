#pragma once

#include "CameraListModel.h"
#include "RtspStreamPipeline.h"
#include "config/AppConfig.h"

#include <QObject>
#include <QString>
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

    // Starts every mosaic pipeline. Deliberately not done in the
    // constructor, so constructing a CameraManager in tests never triggers
    // real network I/O; only main.cpp (and the multi-stream smoke tool)
    // call this.
    Q_INVOKABLE void start();

    AppSinkVideoItem *mosaicVideoItem(const QString &id) const;
    AppSinkVideoItem *fullscreenVideoItem() const;

    Q_INVOKABLE void focus(const QString &id);
    Q_INVOKABLE void enterFullscreen(const QString &id);
    Q_INVOKABLE void exitFullscreen();
    Q_INVOKABLE void switchFullscreenCamera(const QString &id);

signals:
    void fullscreenIdChanged(const QString &id);

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
