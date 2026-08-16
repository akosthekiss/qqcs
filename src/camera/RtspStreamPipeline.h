#pragma once

#include "CameraState.h"
#include "ReconnectScheduler.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstPad GstPad;
typedef struct _GstMessage GstMessage;
// GstSDPMessage is `typedef struct { ... } GstSDPMessage` (anonymous
// struct) in gst/sdp/gstsdpmessage.h, so it can't be forward-declared here
// without redefining it differently -- passed around as void* instead,
// cast back to GstSDPMessage* only in the .cpp that includes the real header.

class AppSinkVideoItem;

// One GStreamer pipeline for one RTSP URL. Built dynamically (rtspsrc's
// codec is only known once its SDP negotiates), decodes video and
// (optionally, see enableAudio()) audio, and renders video through
// AppSinkVideoItem. GstBus messages are drained by a non-blocking 20ms
// timer on the Qt main thread rather than GLib main-loop integration, so
// behavior is identical on macOS and Linux (GLib mainloop integration
// works on Linux but silently never fires on macOS's CF-based dispatcher).
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

    // SPEC §20.1: whether the stream *contains* a decodable audio track,
    // known from the SDP alone (no decoder/sink ever instantiated just to
    // learn this) -- independent of whether audio is actually playing
    // anywhere, so mosaic pipelines (which never call enableAudio(true))
    // still report this correctly.
    bool hasAudio() const { return m_hasAudio; }

    // SPEC §13: fullscreen-only, automatic, no config flag. Attaches/
    // detaches the actual decode+playback branch on demand. Safe to call
    // before the audio pad has appeared (SDP negotiation is async) -- the
    // branch is built as soon as both "wanted" and "pad available" hold.
    void enableAudio(bool enabled);
    bool audioPlaybackError() const { return m_audioPlaybackError; }

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
    void hasAudioChanged(bool hasAudio);

private:
    void setState(CameraState state);
    void pollBus();
    void handleBusMessage(GstMessage *message);
    void handlePadAdded(GstPad *pad);
    void handleDecodebinPadAdded(GstPad *pad);
    void handleOnSdp(void *sdp); // actually a GstSDPMessage*, see note above
    void handleFirstSample();
    void teardownPipeline();
    void buildAndStartPipeline();
    void buildAudioBranch();
    void teardownAudioBranch();
    void handleStreamFailure();
    void retryConnect();

    static void padAddedCallback(GstElement *src, GstPad *pad, void *userData);
    static void decodebinPadAddedCallback(GstElement *decodebin, GstPad *pad, void *userData);
    static void onSdpCallback(GstElement *src, void *sdp, void *userData);
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

    bool m_hasAudio = false;
    bool m_audioEnabled = false;
    bool m_audioPlaybackError = false;
    GstPad *m_audioPad = nullptr; // the RTP audio src pad from rtspsrc, once seen
    QVector<GstElement *> m_audioElements; // queue/decodebin/audioconvert/audioresample/autoaudiosink
    QSet<GstElement *> m_audioElementSet; // same elements, for O(1) bus-error-source isolation

    QTimer m_busPollTimer;
    bool m_seenFirstSample = false;
};
