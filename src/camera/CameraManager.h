#pragma once

#include "CameraListModel.h"
#include "config/AppConfig.h"

#include <QObject>
#include <QString>

// Owns camera lifecycle. In this milestone it only tracks which camera is
// focused/fullscreen and exposes a CameraListModel; GStreamer pipeline
// creation/lazy-start is added once RtspStreamPipeline exists.
class CameraManager : public QObject
{
    Q_OBJECT

public:
    explicit CameraManager(AppConfig config, QObject *parent = nullptr);

    CameraListModel *listModel() const { return m_listModel; }
    QString currentFullscreenId() const { return m_fullscreenId; }

    Q_INVOKABLE void focus(const QString &id);
    Q_INVOKABLE void enterFullscreen(const QString &id);
    Q_INVOKABLE void exitFullscreen();
    Q_INVOKABLE void switchFullscreenCamera(const QString &id);

signals:
    void fullscreenIdChanged(const QString &id);

private:
    AppConfig m_config;
    CameraListModel *m_listModel;
    QString m_fullscreenId;
};
