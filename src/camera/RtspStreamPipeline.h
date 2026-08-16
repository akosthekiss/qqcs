#pragma once

#include "CameraState.h"
#include "ReconnectScheduler.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstPad GstPad;
typedef struct _GstMessage GstMessage;

class AppSinkVideoItem;

// One GStreamer pipeline for one RTSP URL. Built dynamically (rtspsrc's
// codec is only known once its SDP negotiates), decodes video only in this
// milestone (audio playback lands in a later milestone), and renders through
// AppSinkVideoItem. GstBus messages are drained by a non-blocking 20ms timer
// on the Qt main thread rather than GLib main-loop integration, so behavior
// is identical on macOS and Linux (see plan: GLib mainloop integration works
// on Linux but silently never fires on macOS's CF-based dispatcher).
class RtspStreamPipeline : public QObject
{
    Q_OBJECT

public:
    explicit RtspStreamPipeline(QString rtspUrl, QObject *parent = nullptr);
    ~RtspStreamPipeline() override;

    void start();
    void stop();

    CameraState state() const { return m_state; }
    QString rtspUrl() const { return m_rtspUrl; }
    AppSinkVideoItem *videoItem() const { return m_videoItem; }

    // SPEC §33: the scheduler is the single source of truth; these are
    // thin pass-throughs so CameraManager can push them into
    // CameraListModel/diagnostics without exposing ReconnectScheduler
    // itself outside the camera subsystem.
    int reconnectSecondsRemaining() const { return m_reconnectScheduler.secondsRemaining(); }
    int reconnectBackoffSeconds() const { return m_reconnectScheduler.backoffSeconds(); }
    int reconnectCount() const { return m_reconnectScheduler.reconnectCount(); }

signals:
    void stateChanged(CameraState state);
    void reconnectInfoChanged();

private:
    void setState(CameraState state);
    void pollBus();
    void handleBusMessage(GstMessage *message);
    void handlePadAdded(GstPad *pad);
    void handleFirstSample();
    void teardownPipeline();
    void buildAndStartPipeline();
    void handleStreamFailure();
    void retryConnect();

    static void padAddedCallback(GstElement *src, GstPad *pad, void *userData);
    static int newSampleCallback(GstElement *sink, void *userData);

    QString m_rtspUrl;
    CameraState m_state = CameraState::Disconnected;
    AppSinkVideoItem *m_videoItem;
    ReconnectScheduler m_reconnectScheduler;

    GstElement *m_pipeline = nullptr;
    GstElement *m_source = nullptr;
    GstElement *m_appsink = nullptr;
    GstBus *m_bus = nullptr;
    QVector<GstElement *> m_dynamicElements;

    QTimer m_busPollTimer;
    bool m_seenFirstSample = false;
};
