// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include "CameraState.h"
#include "Diagnostics.h"
#include "ReconnectScheduler.h"

#include <QAtomicInteger>
#include <QObject>
#include <QRecursiveMutex>
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
//
// rtspsrc/decodebin emit "pad-added"/"on-sdp" synchronously from their OWN
// internal threads, not the thread that called gst_element_set_state()
// (the Qt GUI thread, which drives every other lifecycle call --
// start/stop/enableAudio -- synchronously, no worker thread anywhere in
// this codebase). An earlier version of this class deferred those
// callbacks' actual work onto the GUI thread via
// QMetaObject::invokeMethod(..., Qt::QueuedConnection) to avoid racing
// against build/teardown -- but rtspsrc starts pushing live RTP data on a
// newly-added pad immediately, and deferring even one Qt event-loop turn
// before linking it left a real window for buffers to arrive at a
// still-unlinked pad, which some elements treat as a fatal stream error
// -- breaking ordinary connections, not just the rapid-switch case that
// motivated deferring in the first place.
//
// m_pipelineMutex (QRecursiveMutex) is what actually serializes this
// instead: every function that touches a GStreamer object or member
// below locks it for its bookkeeping, so pad-added/on-sdp keep running
// synchronously, immediately, on whatever thread emits them (as
// GStreamer expects), while never racing teardownPipeline()/
// teardownAudioBranch() over the same members. That unsynchronized race
// -- not the synchronous timing itself -- was what caused a real,
// reproducible freeze during rapid fullscreen camera switching,
// sometimes with a GStreamer-CRITICAL about disposing an element still
// in PLAYING state, sometimes with no warning at all.
//
// The mutex alone isn't enough, though: gst_element_set_state(...,
// GST_STATE_NULL) on a pipeline containing rtspsrc (or decodebin) can
// itself BLOCK, joining that element's own internal thread -- which is
// exactly the thread that calls handlePadAdded/handleOnSdp/
// handleDecodebinPadAdded, all of which lock this same mutex. Calling
// that blocking teardown while holding the lock (or nested under a
// caller that already does -- QRecursiveMutex lets the same thread
// re-enter, but doesn't release the lock to *other* threads until the
// outermost lock on that thread unwinds) deadlocks: the thread running
// the state change waits for the streaming thread to stop, which waits
// for this thread to unlock, which it can't do until the state change
// returns. This was the second freeze this class produced: the first
// (above) came from the race itself; this one came from fixing that
// race with a mutex naively held across a call that can block on the
// very thread the mutex is meant to keep out.
//
// teardownPipeline() and teardownAudioBranch() fix this the same way in
// spirit -- never call gst_element_set_state(..., GST_STATE_NULL) while
// any thread holds the lock -- but land on different mechanisms because
// one has a caller that can't tolerate it finishing late:
// teardownPipeline() defers every caller that could otherwise reach it
// nested (handleStreamFailure(), see its own callers) so that it is
// itself always entered at lock-depth-zero, then blocks synchronously,
// right there. It has to actually finish before returning:
// ~RtspStreamPipeline() calls stop() -> this and then immediately frees
// `this` (and deletes m_videoItem), and this pipeline's own streaming
// threads call back into `this` via pad probes and signal handlers for
// as long as it keeps running -- returning before it has truly reached
// GST_STATE_NULL would let one of those threads dereference `this`
// after it's freed. teardownAudioBranch() has no such caller (nothing
// blocks on it finishing), so it takes the simpler route of deferring
// the blocking calls themselves to the GUI thread, holding an extra ref
// on each element so they survive even if teardownPipeline() concurrently
// unrefs the whole pipeline they'd otherwise be freed along with.

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
    bool hasAudio() const;

    // SPEC §13: fullscreen-only, automatic, no config flag. Attaches/
    // detaches the actual decode+playback branch on demand. Safe to call
    // before the audio pad has appeared (SDP negotiation is async) -- the
    // branch is built as soon as both "wanted" and "pad available" hold.
    void enableAudio(bool enabled);
    bool audioPlaybackError() const;

    // SPEC §33: the scheduler is the single source of truth; these are
    // thin pass-throughs so CameraManager can push them into
    // CameraListModel/diagnostics without exposing ReconnectScheduler
    // itself outside the camera subsystem.
    int reconnectSecondsRemaining() const { return m_reconnectScheduler.secondsRemaining(); }
    int reconnectBackoffSeconds() const { return m_reconnectScheduler.backoffSeconds(); }
    int reconnectCount() const { return m_reconnectScheduler.reconnectCount(); }

    // SPEC §22.
    Diagnostics diagnostics() const;

    // Public only so the free-function pad-probe trampolines confined to
    // the .cpp (GstPadProbeCallback is a strictly-typed C function
    // pointer, unlike GLib signals' G_CALLBACK blind-cast, so those
    // trampolines can't be class members without leaking GstPadProbeInfo
    // into this header) can reach them.
    void addBitrateBytes(qint64 bytes) { m_bitrateByteAccumulator.fetchAndAddRelaxed(bytes); }
    void noteAppsinkBufferArrived() { m_appsinkBufferCount.fetchAndAddRelaxed(1); }

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

    // See the class comment above for exactly what this protects and why.
    mutable QRecursiveMutex m_pipelineMutex;

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

    // SPEC §22 diagnostics.
    QString m_videoCodec;
    double m_fps = 0.0;
    QString m_audioCodecName;
    QAtomicInteger<qint64> m_bitrateByteAccumulator{ 0 }; // written from a GStreamer thread via a pad probe
    qint64 m_currentBitrateBps = 0;
    QTimer m_bitrateTimer;
    QAtomicInteger<qint64> m_appsinkBufferCount{ 0 }; // written from a GStreamer thread via a pad probe
    QAtomicInteger<qint64> m_pulledSampleCount{ 0 };
};
