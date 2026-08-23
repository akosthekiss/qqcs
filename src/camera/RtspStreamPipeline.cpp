// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "RtspStreamPipeline.h"

#include "AppSinkVideoItem.h"
#include "PlatformGstElements.h"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/sdp/sdp.h>

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QSet>
#include <QUrl>

namespace {
Q_LOGGING_CATEGORY(lcCamera, "qqcs.camera")
constexpr int kRtspProtocolsTcp = 0x00000004; // GST_RTSP_LOWER_TRANS_TCP
constexpr int kRtspLatencyMs = 150;
// rtspsrc's own "timeout"/"tcp-timeout" properties were briefly tightened
// here (5s each, down from the stock 5s/20s) to bound a teardown that has
// to abort a still-CONNECTING rtspsrc -- back when that could block
// gst_element_set_state(..., GST_STATE_NULL) on the Qt GUI thread for the
// full stock timeout. That's no longer the actual risk: the real freeze
// was a data race between GStreamer-callback threads and the GUI thread
// (see RtspStreamPipeline.h's class comment and padAddedCallback's own
// comment), now fixed at its source. Left at rtspsrc's own defaults
// again, since the tightened value turned out to make reconnects more
// frequent than some real cameras/NVRs (with a limited number of
// concurrent RTSP sessions) can tolerate, without addressing the actual
// freeze.

// A previous version of this file made gst_element_set_state(...,
// GST_STATE_NULL) calls wait (bounded, via gst_element_get_state) for the
// transition to actually finish before disposing the element, as a
// defense against "Trying to dispose element X, but it is in PLAYING
// instead of the NULL state" -- a real GStreamer-CRITICAL that turned
// into unrecoverable hangs during rapid camera switching. That wait
// (up to 2s, PER pipeline teardown) turned out to fire often enough in
// ordinary reconnect cycles (a never-fully-connected pipeline being torn
// down is very often still mid-async-transition) to add multiple seconds
// of cumulative GUI-thread delay across several cameras' worth of
// reconnect attempts -- exactly the kind of sluggishness reported after
// adding it, with no such slowdown beforehand.
//
// It's no longer needed: the disposal-while-PLAYING bug's actual cause
// was a data race, not a timing one -- GStreamer signal callbacks
// (pad-added, on-sdp) were mutating this same pipeline's elements from
// GStreamer's own streaming threads, concurrently with teardown running
// on the Qt GUI thread (see this file's padAddedCallback/
// decodebinPadAddedCallback/onSdpCallback and this class's own header
// comment). m_pipelineMutex now serializes exactly that -- teardown and
// any GStreamer-thread callback for the same pipeline can never run
// concurrently -- so nothing else is mutating an element while it's
// being disposed, and GStreamer's own state-change machinery handles
// joining/stopping its internal threads correctly on a plain
// gst_element_set_state(..., GST_STATE_NULL) call, without needing this
// extra wait.
// SPEC §30: Raspberry Pi 5 has no HW video-decode block (Broadcom removed
// it vs Pi4), so avdec_h264/avdec_h265 (software, via gst-libav) is the
// expected decode path there, not just a fallback. Capping each
// decoder's own thread pool keeps N concurrent mosaic decodes from all
// fighting over every core; this property only exists on avdec_* (not
// vtdec/vah264dec), so it's set conditionally below.
constexpr int kDecoderMaxThreads = 2;

// The diagnostics overlay is a TV-facing, potentially-photographable
// display; RTSP URLs commonly embed credentials (rtsp://user:pass@host/...)
// and SPEC §22 only needs the URL for debugging host/path issues, not to
// put a password on screen.
QString maskUrlCredentials(const QString &url)
{
    QUrl parsed(url);
    if (parsed.userName().isEmpty() && parsed.password().isEmpty())
        return url;
    parsed.setUserName(QStringLiteral("***"));
    parsed.setPassword(QStringLiteral("***"));
    return parsed.toString();
}

bool isDecodableAudioCodec(const QString &encodingName)
{
    static const QSet<QString> kSupported = {
        QStringLiteral("PCMA"),          QStringLiteral("PCMU"), QStringLiteral("AAC"),
        QStringLiteral("MPEG4-GENERIC"), QStringLiteral("OPUS"),
    };
    return kSupported.contains(encodingName.toUpper());
}

// Pure function of the GstSDPMessage* rtspsrc's "on-sdp" signal hands the
// callback -- reads only that, writes nothing to *this -- factored out of
// handleOnSdp() purely for readability; handleOnSdp() calls this directly
// and commits the result to its own members itself, still synchronously,
// on whatever thread rtspsrc emits "on-sdp" from (see handleOnSdp()'s own
// comment for why).
void parseSdpAudioInfo(GstSDPMessage *sdp, bool &hasAudio, QString &audioCodecName)
{
    hasAudio = false;
    const guint mediaCount = gst_sdp_message_medias_len(sdp);
    for (guint i = 0; i < mediaCount && !hasAudio; ++i) {
        const GstSDPMedia *media = gst_sdp_message_get_media(sdp, i);
        if (!media || QString::fromUtf8(gst_sdp_media_get_media(media)) != QStringLiteral("audio"))
            continue;
        const guint attrCount = gst_sdp_media_attributes_len(media);
        for (guint a = 0; a < attrCount; ++a) {
            const GstSDPAttribute *attr = gst_sdp_media_get_attribute(media, a);
            if (!attr || QString::fromUtf8(attr->key) != QStringLiteral("rtpmap"))
                continue;
            // rtpmap value looks like "8 PCMA/8000" -- codec name is the
            // second whitespace-separated token, before any '/'.
            const QStringList parts = QString::fromUtf8(attr->value).split(QLatin1Char(' '));
            if (parts.size() < 2)
                continue;
            const QString codecName = parts.at(1).split(QLatin1Char('/')).first();
            audioCodecName = codecName; // diagnostic display, even if not decodable
            if (isDecodableAudioCodec(codecName)) {
                hasAudio = true;
                break;
            }
        }
    }
}

// autoaudiosink silently constructs a plain fakesink as a last resort when
// none of its real candidates (alsasink/pulsesink/pipewiresink/...) can
// actually be opened (e.g. no usable ALSA device on a headless Pi) -- the
// pipeline then plays "successfully" into nowhere, with no
// GST_MESSAGE_ERROR on the bus at all. GstAutoDetect elements decide on
// and add their real child synchronously during their NULL->READY
// transition (before any READY->PAUSED preroll wait that could make the
// call return GST_STATE_CHANGE_ASYNC), so the child is already present as
// soon as gst_element_sync_state_with_parent() returns, even if that call
// itself reports ASYNC.
bool audioSinkFellBackToFakesink(GstElement *autoAudioSink)
{
    GstIterator *it = gst_bin_iterate_elements(GST_BIN(autoAudioSink));
    GValue value = G_VALUE_INIT;
    bool isFakesink = false;
    if (gst_iterator_next(it, &value) == GST_ITERATOR_OK) {
        if (GstElement *child = GST_ELEMENT(g_value_get_object(&value))) {
            GstElementFactory *factory = gst_element_get_factory(child);
            isFakesink = factory && g_str_equal(GST_OBJECT_NAME(factory), "fakesink");
        }
        g_value_unset(&value);
    }
    gst_iterator_free(it);
    return isFakesink;
}

// Free functions, not class members: GstPadProbeCallback is a strictly-
// typed C function pointer (gst_pad_add_probe calls through it directly,
// unlike GLib signals' G_CALLBACK blind-cast), so keeping GstPadProbeInfo
// confined to this .cpp is what keeps it out of RtspStreamPipeline.h.
GstPadProbeReturn onBitrateProbe(GstPad *, GstPadProbeInfo *info, gpointer userData)
{
    if (GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info))
        static_cast<RtspStreamPipeline *>(userData)->addBitrateBytes(static_cast<qint64>(gst_buffer_get_size(buffer)));
    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn onAppsinkBufferProbe(GstPad *, GstPadProbeInfo *, gpointer userData)
{
    static_cast<RtspStreamPipeline *>(userData)->noteAppsinkBufferArrived();
    return GST_PAD_PROBE_OK;
}
}

RtspStreamPipeline::RtspStreamPipeline(QString rtspUrl, QObject *parent)
    : QObject(parent), m_rtspUrl(std::move(rtspUrl)), m_videoItem(new AppSinkVideoItem), m_reconnectScheduler(this)
{
    m_busPollTimer.setInterval(20);
    connect(&m_busPollTimer, &QTimer::timeout, this, &RtspStreamPipeline::pollBus);
    connect(&m_reconnectScheduler, &ReconnectScheduler::attemptDue, this, &RtspStreamPipeline::retryConnect);
    connect(&m_reconnectScheduler, &ReconnectScheduler::tick, this, &RtspStreamPipeline::reconnectInfoChanged);

    m_bitrateTimer.setInterval(1000);
    connect(&m_bitrateTimer, &QTimer::timeout, this,
            [this] { m_currentBitrateBps = m_bitrateByteAccumulator.fetchAndStoreRelaxed(0) * 8; });
    m_bitrateTimer.start();
}

RtspStreamPipeline::~RtspStreamPipeline()
{
    stop();
    delete m_videoItem;
}

void RtspStreamPipeline::start()
{
    if (m_pipeline)
        return;
    setState(CameraState::Connecting);
    buildAndStartPipeline();
}

void RtspStreamPipeline::stop()
{
    m_busPollTimer.stop();
    teardownPipeline();
    setState(CameraState::Disconnected);
}

void RtspStreamPipeline::retryConnect()
{
    m_reconnectScheduler.onAttemptStarted(); // hides countdown, goes Connecting (SPEC §20.2)
    setState(CameraState::Connecting);
    buildAndStartPipeline();
}

void RtspStreamPipeline::handleStreamFailure()
{
    m_busPollTimer.stop();
    teardownPipeline();
    setState(CameraState::Lost);
    m_reconnectScheduler.onConnectionLost();
}

void RtspStreamPipeline::buildAndStartPipeline()
{
    QMutexLocker locker(&m_pipelineMutex);
    // start() already guards against a live m_pipeline, but retryConnect()
    // (the other caller, reached via ReconnectScheduler::attemptDue) does
    // not -- if it were ever invoked while a pipeline is still live, this
    // would silently overwrite m_pipeline/m_source/m_appsink out from
    // under any in-flight callback still holding the old pointers,
    // orphaning the old GstElements and leaving pad-added/on-sdp for the
    // old rtspsrc operating on the new pipeline's members. Cheap to guard
    // here unconditionally rather than rely on every caller doing it.
    if (m_pipeline)
        return;
    m_pipeline = gst_pipeline_new(nullptr);
    m_source = gst_element_factory_make("rtspsrc", nullptr);
    m_appsink = gst_element_factory_make("appsink", nullptr);

    if (!m_pipeline || !m_source || !m_appsink) {
        qCWarning(lcCamera) << "Failed to create base pipeline elements for" << m_rtspUrl;
        // This function is already holding m_pipelineMutex -- see
        // teardownPipeline()'s comment for why handleStreamFailure()
        // (which calls it) must never run nested under this lock.
        QMetaObject::invokeMethod(this, &RtspStreamPipeline::handleStreamFailure, Qt::QueuedConnection);
        return;
    }

    g_object_set(m_source,
                 "location", m_rtspUrl.toUtf8().constData(),
                 "latency", kRtspLatencyMs,
                 "protocols", kRtspProtocolsTcp,
                 nullptr);

    GstCaps *caps = gst_caps_from_string("video/x-raw,format=RGBA");
    g_object_set(m_appsink,
                 "caps", caps,
                 "emit-signals", TRUE,
                 "max-buffers", 1,
                 "drop", TRUE,
                 "sync", FALSE,
                 nullptr);
    gst_caps_unref(caps);

    g_signal_connect(m_source, "pad-added", G_CALLBACK(&RtspStreamPipeline::padAddedCallback), this);
    g_signal_connect(m_source, "on-sdp", G_CALLBACK(&RtspStreamPipeline::onSdpCallback), this);
    g_signal_connect(m_appsink, "new-sample", G_CALLBACK(&RtspStreamPipeline::newSampleCallback), this);

    GstPad *appsinkSinkPad = gst_element_get_static_pad(m_appsink, "sink");
    gst_pad_add_probe(appsinkSinkPad, GST_PAD_PROBE_TYPE_BUFFER, &onAppsinkBufferProbe, this, nullptr);
    gst_object_unref(appsinkSinkPad);

    gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_appsink, nullptr);

    m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    m_bitrateByteAccumulator.storeRelaxed(0);
    m_currentBitrateBps = 0;
    m_appsinkBufferCount.storeRelaxed(0);
    m_pulledSampleCount.storeRelaxed(0);
    m_videoCodec.clear();
    m_fps = 0.0;
    m_audioCodecName.clear();

    const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qCWarning(lcCamera) << "Failed to start pipeline for" << m_rtspUrl;
        // See the element-creation failure branch above.
        QMetaObject::invokeMethod(this, &RtspStreamPipeline::handleStreamFailure, Qt::QueuedConnection);
        return;
    }

    m_busPollTimer.start();
}

void RtspStreamPipeline::teardownPipeline()
{
    // gst_element_set_state(..., GST_STATE_NULL) on a pipeline containing
    // rtspsrc can BLOCK: rtspsrc's stop() joins its own internal
    // connection-management thread, which is exactly the thread that
    // calls handlePadAdded/handleOnSdp -- and both of those start by
    // locking m_pipelineMutex. Calling this function while still holding
    // that lock (directly, or nested through a caller that already
    // holds it -- QRecursiveMutex only lets the *same* thread re-enter,
    // it doesn't actually release the lock to other threads until the
    // outermost QMutexLocker on that thread unwinds) would make a state
    // change that needs to join that thread wait forever for a thread
    // that itself is waiting forever for this one to unlock. So every
    // caller that could otherwise be nested (handleStreamFailure(),
    // reachable from handleBusMessage()/buildAndStartPipeline()/
    // handlePadAdded(), all of which lock this mutex themselves) defers
    // through a fresh, unnested QueuedConnection first -- see
    // handleStreamFailure()'s callers for that half of the guarantee.
    // That leaves this function free to actually block: the member
    // bookkeeping below still runs under the lock (cheap, never blocks),
    // but is released *before* the blocking GStreamer call, which then
    // runs synchronously, immediately, at guaranteed lock-depth-zero.
    //
    // This has to be synchronous, not deferred to later like
    // teardownAudioBranch() below: ~RtspStreamPipeline() calls stop() ->
    // this function and then immediately frees `this` (and deletes
    // m_videoItem) -- and GStreamer streaming threads for this pipeline
    // (rtspsrc's, decodebin's, the decoder's) call back into `this` via
    // pad probes (onBitrateProbe/onAppsinkBufferProbe) and signal
    // handlers for as long as the pipeline keeps running. Returning here
    // before the pipeline has actually reached GST_STATE_NULL (and thus
    // joined all of those threads) would let one of them dereference
    // `this` after it's already freed -- exactly the
    // heap-use-after-free ASan caught in an earlier version of this fix
    // that deferred the blocking call unconditionally.
    GstElement *pipeline;
    {
        QMutexLocker locker(&m_pipelineMutex);
        pipeline = m_pipeline;
        m_pipeline = nullptr;
        m_source = nullptr;
        m_appsink = nullptr;
        if (m_bus) {
            gst_object_unref(m_bus);
            m_bus = nullptr;
        }
        if (m_audioPad) {
            gst_object_unref(m_audioPad);
            m_audioPad = nullptr;
        }
        m_audioElements.clear();
        m_audioElementSet.clear();
        m_audioPlaybackError = false;
        m_dynamicElements.clear();
    }
    if (!pipeline)
        return;
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

void RtspStreamPipeline::setState(CameraState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void RtspStreamPipeline::pollBus()
{
    // Re-check m_bus at the top of every iteration, not just once before
    // the loop: handleBusMessage() can itself call handleStreamFailure(),
    // which tears down the pipeline (unreffing m_bus and setting it to
    // nullptr) from inside this same loop, on the error/EOS path. Without
    // this, the loop's own condition would call gst_bus_timed_pop_filtered
    // on a just-nulled bus, tripping a GST_IS_BUS() assertion every time a
    // stream failure is handled.
    while (m_bus) {
        GstMessage *msg = gst_bus_timed_pop_filtered(
            m_bus, 0,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED
                                        | GST_MESSAGE_WARNING));
        if (!msg)
            break;
        handleBusMessage(msg);
        gst_message_unref(msg);
    }
}

void RtspStreamPipeline::handleBusMessage(GstMessage *message)
{
    // Runs on the GUI thread (via pollBus()'s timer), but still needs
    // the lock: it reads m_audioElementSet directly below, which
    // handlePadAdded/buildAudioBranch (running on a GStreamer thread,
    // see their own comments) can be writing concurrently.
    QMutexLocker locker(&m_pipelineMutex);
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        GError *err = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(message, &err, &debug);
        // SPEC §13: audio failures must never affect video/CameraState.
        // Identify the failing element by direct pointer membership rather
        // than name-prefix matching, so this can't misfire on a
        // coincidentally-named video element.
        if (m_audioElementSet.contains(GST_ELEMENT(GST_MESSAGE_SRC(message)))) {
            qCWarning(lcCamera).noquote() << m_rtspUrl << "audio playback error:" << (err ? err->message : "unknown");
            if (err)
                g_error_free(err);
            if (debug)
                g_free(debug);
            m_audioPlaybackError = true;
            teardownAudioBranch();
            break;
        }
        qCWarning(lcCamera).noquote() << m_rtspUrl << "pipeline error:" << (err ? err->message : "unknown");
        if (err)
            g_error_free(err);
        if (debug)
            g_free(debug);
        // This function is already holding m_pipelineMutex -- see
        // teardownPipeline()'s comment for why handleStreamFailure() must
        // never run nested under this lock.
        QMetaObject::invokeMethod(this, &RtspStreamPipeline::handleStreamFailure, Qt::QueuedConnection);
        break;
    }
    case GST_MESSAGE_EOS:
        qCWarning(lcCamera).noquote() << m_rtspUrl << "end of stream";
        QMetaObject::invokeMethod(this, &RtspStreamPipeline::handleStreamFailure, Qt::QueuedConnection);
        break;
    case GST_MESSAGE_WARNING: {
        GError *err = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_warning(message, &err, &debug);
        qCWarning(lcCamera).noquote() << m_rtspUrl << "pipeline warning:" << (err ? err->message : "unknown");
        if (err)
            g_error_free(err);
        if (debug)
            g_free(debug);
        break;
    }
    case GST_MESSAGE_STATE_CHANGED:
        break;
    default:
        break;
    }
}

void RtspStreamPipeline::padAddedCallback(GstElement *, GstPad *pad, void *userData)
{
    static_cast<RtspStreamPipeline *>(userData)->handlePadAdded(pad);
}

void RtspStreamPipeline::handlePadAdded(GstPad *pad)
{
    // Runs synchronously on whatever thread rtspsrc emits pad-added from
    // (its own connection-management thread, not the Qt GUI thread that
    // owns start/stop/enableAudio/teardown) -- deliberately NOT deferred:
    // rtspsrc starts pushing live RTP data on this pad right away, and
    // an app that doesn't link it immediately risks that data arriving
    // at a still-unlinked pad, which some elements treat as a fatal
    // stream error. m_pipelineMutex (see the class comment) is what
    // actually keeps this safe against teardownPipeline()/
    // teardownAudioBranch() running concurrently on the GUI thread,
    // without needing to delay anything.
    QMutexLocker locker(&m_pipelineMutex);
    // teardownPipeline() nulls m_pipeline under a short-lived lock
    // *before* actually blocking on gst_element_set_state(..., NULL)
    // (see its own comment) -- so this callback can genuinely observe a
    // torn-down pipeline if it acquires the lock in that window, e.g. a
    // rapid camera switch tearing this pipeline down on another thread.
    // Bail out before touching m_pipeline/m_appsink below; there is
    // nothing left to link into.
    if (!m_pipeline)
        return;
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, nullptr);
    if (!caps)
        return;
    // A non-NULL GstCaps can still have zero structures (e.g. an "ANY"/
    // empty caps from querying a pad that's already been unlinked or
    // hit a stream error by the time this runs) -- now more reachable
    // than before this handler started being deferred to the GUI thread
    // (see padAddedCallback's comment), widening the window between the
    // pad appearing and this code actually running. gst_caps_get_structure
    // on an empty caps trips a GStreamer-CRITICAL assertion instead of
    // just returning NULL, so this must be checked explicitly first.
    if (gst_caps_get_size(caps) == 0) {
        gst_caps_unref(caps);
        return;
    }

    const GstStructure *s = gst_caps_get_structure(caps, 0);
    const gchar *media = gst_structure_get_string(s, "media");
    const gchar *encodingName = gst_structure_get_string(s, "encoding-name");
    if (!media || !encodingName) {
        gst_caps_unref(caps);
        return;
    }
    const QString mediaType = QString::fromUtf8(media);
    const QString codec = QString::fromUtf8(encodingName);
    // Read while `s` (a view into `caps`) is still valid -- must happen
    // before gst_caps_unref() below, which may free it immediately.
    double fps = 0.0;
    if (const gchar *framerateStr = gst_structure_get_string(s, "a-framerate"))
        fps = QString::fromUtf8(framerateStr).toDouble();
    gst_caps_unref(caps);

    if (mediaType == QStringLiteral("audio")) {
        if (m_audioPad)
            gst_object_unref(m_audioPad);
        m_audioPad = GST_PAD(gst_object_ref(pad));
        if (m_audioEnabled)
            buildAudioBranch();
        return;
    }
    if (mediaType != QStringLiteral("video"))
        return;

    m_videoCodec = codec;
    m_fps = fps;

    const bool isH265 = codec.compare(QStringLiteral("H265"), Qt::CaseInsensitive) == 0;
    const QString depayName = isH265 ? QStringLiteral("rtph265depay") : QStringLiteral("rtph264depay");
    const QString parseName = isH265 ? QStringLiteral("h265parse") : QStringLiteral("h264parse");
    const QString decoderName = PlatformGstElements::pickVideoDecoder(codec);

    if (decoderName.isEmpty()) {
        qCWarning(lcCamera) << "No usable decoder for codec" << codec << "on" << m_rtspUrl;
        // handleStreamFailure() stops m_busPollTimer (a GUI-thread-affine
        // QTimer, unsafe to touch from this GStreamer thread) and calls
        // teardownPipeline() -- deferring the whole call, not just the
        // teardown, keeps this callback fast and every bit of that
        // GUI-owned state on the thread that actually owns it. Nothing
        // here needs to happen before rtspsrc's pad-added returns; unlike
        // linking, error handling isn't racing live RTP data.
        QMetaObject::invokeMethod(this, &RtspStreamPipeline::handleStreamFailure, Qt::QueuedConnection);
        return;
    }

    GstElement *depay = gst_element_factory_make(depayName.toUtf8().constData(), nullptr);
    GstElement *parse = gst_element_factory_make(parseName.toUtf8().constData(), nullptr);
    GstElement *decoder = gst_element_factory_make(decoderName.toUtf8().constData(), nullptr);
    GstElement *convert = gst_element_factory_make("videoconvert", nullptr);

    if (!depay || !parse || !decoder || !convert) {
        qCWarning(lcCamera) << "Failed to instantiate decode chain for" << m_rtspUrl;
        // See the decoderName.isEmpty() branch above for why this is deferred.
        QMetaObject::invokeMethod(this, &RtspStreamPipeline::handleStreamFailure, Qt::QueuedConnection);
        return;
    }

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(decoder), "max-threads"))
        g_object_set(decoder, "max-threads", kDecoderMaxThreads, nullptr);

    gst_bin_add_many(GST_BIN(m_pipeline), depay, parse, decoder, convert, nullptr);
    if (!gst_element_link_many(depay, parse, decoder, convert, m_appsink, nullptr)) {
        qCWarning(lcCamera) << "Failed to link decode chain for" << m_rtspUrl;
        // See the decoderName.isEmpty() branch above for why this is deferred.
        QMetaObject::invokeMethod(this, &RtspStreamPipeline::handleStreamFailure, Qt::QueuedConnection);
        return;
    }

    gst_element_sync_state_with_parent(depay);
    gst_element_sync_state_with_parent(parse);
    gst_element_sync_state_with_parent(decoder);
    gst_element_sync_state_with_parent(convert);

    GstPad *sinkPad = gst_element_get_static_pad(depay, "sink");
    gst_pad_link(pad, sinkPad);
    gst_object_unref(sinkPad);

    GstPad *depaySrcPad = gst_element_get_static_pad(depay, "src");
    gst_pad_add_probe(depaySrcPad, GST_PAD_PROBE_TYPE_BUFFER, &onBitrateProbe, this, nullptr);
    gst_object_unref(depaySrcPad);

    m_dynamicElements = { depay, parse, decoder, convert };
}

int RtspStreamPipeline::newSampleCallback(GstElement *sink, void *userData)
{
    auto *self = static_cast<RtspStreamPipeline *>(userData);
    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample)
        return static_cast<int>(GST_FLOW_ERROR);

    self->m_videoItem->pushSample(sample); // takes ownership
    // fetchAndAddRelaxed() returns the *previous* value -- 0 means this
    // was the first sample pulled since the last buildAndStartPipeline()
    // reset, atomically and without a separate (and, until now, racy --
    // written from this GStreamer thread but reset from the GUI thread
    // with no lock) bool flag to keep in sync.
    if (self->m_pulledSampleCount.fetchAndAddRelaxed(1) == 0)
        QMetaObject::invokeMethod(self, &RtspStreamPipeline::handleFirstSample, Qt::QueuedConnection);
    return static_cast<int>(GST_FLOW_OK);
}

void RtspStreamPipeline::handleFirstSample()
{
    setState(CameraState::Live);
    m_reconnectScheduler.onConnectSucceeded();
}

void RtspStreamPipeline::onSdpCallback(GstElement *, void *sdp, void *userData)
{
    static_cast<RtspStreamPipeline *>(userData)->handleOnSdp(sdp);
}

void RtspStreamPipeline::handleOnSdp(void *sdpVoid)
{
    // SPEC §20.1: capability only, from the SDP alone -- no decoder or
    // sink is ever instantiated just to learn this, so it's free even for
    // mosaic pipelines that will never call enableAudio(true). Runs
    // synchronously on rtspsrc's own thread, like handlePadAdded above --
    // see its comment and the class comment for why, and what
    // m_pipelineMutex actually protects here.
    QMutexLocker locker(&m_pipelineMutex);
    // Same rapid-switch race as handlePadAdded's guard above: if the
    // pipeline was already torn down by the time this runs, there's
    // nothing meaningful left to update.
    if (!m_pipeline)
        return;
    bool hasAudio = false;
    QString audioCodecName;
    parseSdpAudioInfo(static_cast<GstSDPMessage *>(sdpVoid), hasAudio, audioCodecName);
    if (!audioCodecName.isEmpty())
        m_audioCodecName = audioCodecName;
    if (hasAudio != m_hasAudio) {
        m_hasAudio = hasAudio;
        emit hasAudioChanged(m_hasAudio);
    }
}

void RtspStreamPipeline::enableAudio(bool enabled)
{
    // GUI-thread-only entry point, but m_audioEnabled/m_audioPad are also
    // read from handlePadAdded on a GStreamer thread, so this still needs
    // the lock (QRecursiveMutex: safe to also take it again inside
    // buildAudioBranch()/teardownAudioBranch() below).
    QMutexLocker locker(&m_pipelineMutex);
    if (m_audioEnabled == enabled)
        return;
    m_audioEnabled = enabled;
    if (enabled) {
        if (m_audioPad)
            buildAudioBranch();
        // else: pad hasn't appeared yet -- handlePadAdded() builds it once it does.
    } else {
        teardownAudioBranch();
    }
}

void RtspStreamPipeline::buildAudioBranch()
{
    // Called either from enableAudio(true) (GUI thread) or from
    // handlePadAdded (a GStreamer thread, already holding the lock --
    // QRecursiveMutex makes this safe).
    QMutexLocker locker(&m_pipelineMutex);
    if (!m_audioElements.isEmpty() || !m_pipeline || !m_audioPad)
        return;

    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *decodebin = gst_element_factory_make("decodebin", nullptr);
    GstElement *convert = gst_element_factory_make("audioconvert", nullptr);
    GstElement *resample = gst_element_factory_make("audioresample", nullptr);
    GstElement *sink = gst_element_factory_make("autoaudiosink", nullptr);

    if (!queue || !decodebin || !convert || !resample || !sink) {
        qCWarning(lcCamera) << "Failed to instantiate audio branch for" << m_rtspUrl;
        m_audioPlaybackError = true;
        return;
    }

    gst_bin_add_many(GST_BIN(m_pipeline), queue, decodebin, convert, resample, sink, nullptr);
    gst_element_link(queue, decodebin);
    gst_element_link_many(convert, resample, sink, nullptr);
    // decodebin's output pad only appears once it auto-plugs a decoder for
    // whatever codec is actually negotiated -- a second level of dynamic
    // pad-linking, separate from rtspsrc's own pad-added above.
    g_signal_connect(decodebin, "pad-added", G_CALLBACK(&RtspStreamPipeline::decodebinPadAddedCallback), this);

    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(decodebin);
    gst_element_sync_state_with_parent(convert);
    gst_element_sync_state_with_parent(resample);
    gst_element_sync_state_with_parent(sink);

    GstPad *queueSinkPad = gst_element_get_static_pad(queue, "sink");
    gst_pad_link(m_audioPad, queueSinkPad);
    gst_object_unref(queueSinkPad);

    m_audioElements = { queue, decodebin, convert, resample, sink };
    m_audioElementSet = { queue, decodebin, convert, resample, sink };
    m_audioPlaybackError = false;

    // See audioSinkFellBackToFakesink()'s comment: this is the one audio-
    // branch failure mode that never raises a bus error on its own, so it
    // needs its own explicit check right here instead.
    if (audioSinkFellBackToFakesink(sink)) {
        qCWarning(lcCamera).noquote() << m_rtspUrl
                                       << "audio playback error: no usable audio output device (autoaudiosink "
                                          "fell back to fakesink)";
        m_audioPlaybackError = true;
        teardownAudioBranch();
    }
}

void RtspStreamPipeline::teardownAudioBranch()
{
    // Called from the GUI thread (enableAudio(false)) or from
    // handleBusMessage's audio-error path (also GUI thread, via
    // pollBus's timer, already holding the lock) or the fakesink-
    // fallback check inside buildAudioBranch (a GStreamer thread,
    // already holding the lock) -- QRecursiveMutex makes all of these
    // safe to nest for the bookkeeping below, but decodebin (like
    // rtspsrc) runs its own internal streaming thread that
    // handleDecodebinPadAdded's lock is guarding against, so the actual
    // gst_element_set_state(..., GST_STATE_NULL) calls have the exact
    // same self-deadlock risk described in teardownPipeline()'s comment.
    // Unlike teardownPipeline(), nothing needs this to have actually
    // finished before returning (no destructor is waiting on it, and
    // decodebin's own thread doesn't reach back into any *other*
    // member than these same elements), so it's fine -- and necessary,
    // to dodge the deadlock without auditing every caller for nesting
    // the way teardownPipeline() needed -- to defer the blocking calls
    // to the GUI thread instead of forcing lock-depth-zero on entry.
    //
    // gst_object_ref() on each element before releasing the lock (and
    // unref after) keeps them alive even if teardownPipeline() runs
    // concurrently on the GUI thread and unrefs the *whole pipeline*
    // (these elements' other owner) before this deferred call gets a
    // turn: that unref can drop the bin's own ref and dispose everything
    // in it, but our extra ref delays that until this lambda releases
    // its own, so gst_element_set_state/gst_element_get_parent/
    // gst_bin_remove below always see live objects -- just possibly
    // already NULL-state and already parentless, both handled as
    // no-ops. gst_element_get_parent() (rather than GST_BIN(m_pipeline))
    // finds each element's current bin, without this function needing
    // to hold a reference to m_pipeline past its own critical section.
    QVector<GstElement *> elements;
    {
        QMutexLocker locker(&m_pipelineMutex);
        if (m_audioElements.isEmpty())
            return;
        elements = m_audioElements;
        for (GstElement *element : elements)
            gst_object_ref(element);
        m_audioElements.clear();
        m_audioElementSet.clear();
    }
    QMetaObject::invokeMethod(
        qApp,
        [elements] {
            for (GstElement *element : elements) {
                gst_element_set_state(element, GST_STATE_NULL);
                if (GstObject *parent = gst_element_get_parent(element)) {
                    gst_bin_remove(GST_BIN(parent), element);
                    gst_object_unref(parent);
                }
                gst_object_unref(element);
            }
        },
        Qt::QueuedConnection);
}

void RtspStreamPipeline::decodebinPadAddedCallback(GstElement *, GstPad *pad, void *userData)
{
    static_cast<RtspStreamPipeline *>(userData)->handleDecodebinPadAdded(pad);
}

void RtspStreamPipeline::handleDecodebinPadAdded(GstPad *pad)
{
    // Same reasoning as handlePadAdded above: decodebin's pad-added also
    // fires synchronously on its own streaming thread, with live decoded
    // audio about to flow, so this must link it immediately rather than
    // defer -- m_pipelineMutex (not deferral) is what keeps this safe
    // against teardownAudioBranch() reading/clearing m_audioElements
    // concurrently from the GUI thread.
    QMutexLocker locker(&m_pipelineMutex);
    if (m_audioElements.size() < 3)
        return;
    GstElement *convert = m_audioElements.at(2); // audioconvert
    GstPad *sinkPad = gst_element_get_static_pad(convert, "sink");
    if (!gst_pad_is_linked(sinkPad))
        gst_pad_link(pad, sinkPad);
    gst_object_unref(sinkPad);
}

Diagnostics RtspStreamPipeline::diagnostics() const
{
    // Called from the GUI thread (CameraManager), but m_videoCodec/m_fps/
    // m_audioCodecName are written from a GStreamer thread in
    // handlePadAdded/handleOnSdp -- lock for a consistent snapshot rather
    // than a possible mix of old/new values across fields.
    QMutexLocker locker(&m_pipelineMutex);
    Diagnostics d;
    d.videoCodec = m_videoCodec;
    d.width = m_videoItem->nativeWidth();
    d.height = m_videoItem->nativeHeight();
    d.fps = m_fps;
    d.bitrateBps = m_currentBitrateBps;
    d.audioCodec = m_audioCodecName;
    d.rtspTransport = QStringLiteral("TCP"); // configured value (SPEC §31); not queried from rtspsrc internals
    d.droppedFrames = qMax<qint64>(0, m_appsinkBufferCount.loadRelaxed() - m_pulledSampleCount.loadRelaxed());
    d.reconnectCount = reconnectCount();
    d.reconnectBackoffSeconds = reconnectBackoffSeconds();
    d.reconnectCountdownSeconds = reconnectSecondsRemaining();
    d.rtspUrl = maskUrlCredentials(m_rtspUrl);
    return d;
}
