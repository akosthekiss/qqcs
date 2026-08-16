#include "RtspStreamPipeline.h"

#include "AppSinkVideoItem.h"
#include "PlatformGstElements.h"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/sdp/sdp.h>

#include <QLoggingCategory>
#include <QSet>
#include <QUrl>

namespace {
Q_LOGGING_CATEGORY(lcCamera, "qqcs.camera")
constexpr int kRtspProtocolsTcp = 0x00000004; // GST_RTSP_LOWER_TRANS_TCP
constexpr int kRtspLatencyMs = 150;
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
    m_pipeline = gst_pipeline_new(nullptr);
    m_source = gst_element_factory_make("rtspsrc", nullptr);
    m_appsink = gst_element_factory_make("appsink", nullptr);

    if (!m_pipeline || !m_source || !m_appsink) {
        qCWarning(lcCamera) << "Failed to create base pipeline elements for" << m_rtspUrl;
        handleStreamFailure();
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
    m_seenFirstSample = false;
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
        handleStreamFailure();
        return;
    }

    m_busPollTimer.start();
}

void RtspStreamPipeline::teardownPipeline()
{
    if (m_pipeline)
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
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
    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    m_source = nullptr;
    m_appsink = nullptr;
    m_dynamicElements.clear();
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
    if (!m_bus)
        return;
    while (GstMessage *msg = gst_bus_timed_pop_filtered(
               m_bus, 0,
               static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED
                                           | GST_MESSAGE_WARNING))) {
        handleBusMessage(msg);
        gst_message_unref(msg);
    }
}

void RtspStreamPipeline::handleBusMessage(GstMessage *message)
{
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
        handleStreamFailure();
        break;
    }
    case GST_MESSAGE_EOS:
        qCWarning(lcCamera).noquote() << m_rtspUrl << "end of stream";
        handleStreamFailure();
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
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, nullptr);
    if (!caps)
        return;

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
        handleStreamFailure();
        return;
    }

    GstElement *depay = gst_element_factory_make(depayName.toUtf8().constData(), nullptr);
    GstElement *parse = gst_element_factory_make(parseName.toUtf8().constData(), nullptr);
    GstElement *decoder = gst_element_factory_make(decoderName.toUtf8().constData(), nullptr);
    GstElement *convert = gst_element_factory_make("videoconvert", nullptr);

    if (!depay || !parse || !decoder || !convert) {
        qCWarning(lcCamera) << "Failed to instantiate decode chain for" << m_rtspUrl;
        handleStreamFailure();
        return;
    }

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(decoder), "max-threads"))
        g_object_set(decoder, "max-threads", kDecoderMaxThreads, nullptr);

    gst_bin_add_many(GST_BIN(m_pipeline), depay, parse, decoder, convert, nullptr);
    if (!gst_element_link_many(depay, parse, decoder, convert, m_appsink, nullptr)) {
        qCWarning(lcCamera) << "Failed to link decode chain for" << m_rtspUrl;
        handleStreamFailure();
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
    self->m_pulledSampleCount.fetchAndAddRelaxed(1);

    if (!self->m_seenFirstSample) {
        self->m_seenFirstSample = true;
        QMetaObject::invokeMethod(self, &RtspStreamPipeline::handleFirstSample, Qt::QueuedConnection);
    }
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
    auto *sdp = static_cast<GstSDPMessage *>(sdpVoid);
    // SPEC §20.1: capability only, from the SDP alone -- no decoder or
    // sink is ever instantiated just to learn this, so it's free even for
    // mosaic pipelines that will never call enableAudio(true).
    bool foundDecodableAudio = false;
    const guint mediaCount = gst_sdp_message_medias_len(sdp);
    for (guint i = 0; i < mediaCount && !foundDecodableAudio; ++i) {
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
            m_audioCodecName = codecName; // diagnostic display, even if not decodable
            if (isDecodableAudioCodec(codecName)) {
                foundDecodableAudio = true;
                break;
            }
        }
    }

    if (foundDecodableAudio != m_hasAudio) {
        m_hasAudio = foundDecodableAudio;
        emit hasAudioChanged(m_hasAudio);
    }
}

void RtspStreamPipeline::enableAudio(bool enabled)
{
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
}

void RtspStreamPipeline::teardownAudioBranch()
{
    if (m_audioElements.isEmpty())
        return;
    for (GstElement *element : std::as_const(m_audioElements)) {
        gst_element_set_state(element, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), element);
    }
    m_audioElements.clear();
    m_audioElementSet.clear();
}

void RtspStreamPipeline::decodebinPadAddedCallback(GstElement *, GstPad *pad, void *userData)
{
    static_cast<RtspStreamPipeline *>(userData)->handleDecodebinPadAdded(pad);
}

void RtspStreamPipeline::handleDecodebinPadAdded(GstPad *pad)
{
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
    Diagnostics d;
    d.videoCodec = m_videoCodec;
    d.width = m_videoItem->nativeWidth();
    d.height = m_videoItem->nativeHeight();
    d.fps = m_fps;
    d.bitrateBps = m_currentBitrateBps;
    d.audioCodec = m_audioCodecName;
    d.rtspTransport = QStringLiteral("TCP"); // configured value (SPEC §31); not queried from rtspsrc internals
    d.latencyMs = -1; // N/A -- see Diagnostics.h
    d.droppedFrames = qMax<qint64>(0, m_appsinkBufferCount.loadRelaxed() - m_pulledSampleCount.loadRelaxed());
    d.reconnectCount = reconnectCount();
    d.reconnectBackoffSeconds = reconnectBackoffSeconds();
    d.reconnectCountdownSeconds = reconnectSecondsRemaining();
    d.rtspUrl = maskUrlCredentials(m_rtspUrl);
    return d;
}
