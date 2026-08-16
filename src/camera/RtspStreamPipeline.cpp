#include "RtspStreamPipeline.h"

#include "AppSinkVideoItem.h"
#include "PlatformGstElements.h"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <QLoggingCategory>

namespace {
Q_LOGGING_CATEGORY(lcCamera, "qqcs.camera")
constexpr int kRtspProtocolsTcp = 0x00000004; // GST_RTSP_LOWER_TRANS_TCP
constexpr int kRtspLatencyMs = 150;
}

RtspStreamPipeline::RtspStreamPipeline(QString rtspUrl, QObject *parent)
    : QObject(parent), m_rtspUrl(std::move(rtspUrl)), m_videoItem(new AppSinkVideoItem)
{
    m_busPollTimer.setInterval(20);
    connect(&m_busPollTimer, &QTimer::timeout, this, &RtspStreamPipeline::pollBus);
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

    m_pipeline = gst_pipeline_new(nullptr);
    m_source = gst_element_factory_make("rtspsrc", nullptr);
    m_appsink = gst_element_factory_make("appsink", nullptr);

    if (!m_pipeline || !m_source || !m_appsink) {
        qCWarning(lcCamera) << "Failed to create base pipeline elements for" << m_rtspUrl;
        teardownPipeline();
        setState(CameraState::Lost);
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
    g_signal_connect(m_appsink, "new-sample", G_CALLBACK(&RtspStreamPipeline::newSampleCallback), this);

    gst_bin_add_many(GST_BIN(m_pipeline), m_source, m_appsink, nullptr);

    m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    m_seenFirstSample = false;
    setState(CameraState::Connecting);

    const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qCWarning(lcCamera) << "Failed to start pipeline for" << m_rtspUrl;
        teardownPipeline();
        setState(CameraState::Lost);
        return;
    }

    m_busPollTimer.start();
}

void RtspStreamPipeline::stop()
{
    m_busPollTimer.stop();
    teardownPipeline();
    setState(CameraState::Disconnected);
}

void RtspStreamPipeline::teardownPipeline()
{
    if (m_pipeline)
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    if (m_bus) {
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }
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
        qCWarning(lcCamera).noquote() << m_rtspUrl << "pipeline error:" << (err ? err->message : "unknown");
        if (err)
            g_error_free(err);
        if (debug)
            g_free(debug);
        setState(CameraState::Lost);
        break;
    }
    case GST_MESSAGE_EOS:
        qCWarning(lcCamera).noquote() << m_rtspUrl << "end of stream";
        setState(CameraState::Lost);
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
    if (!media || QString::fromUtf8(media) != QStringLiteral("video") || !encodingName) {
        gst_caps_unref(caps);
        return;
    }
    const QString codec = QString::fromUtf8(encodingName);
    gst_caps_unref(caps);

    const bool isH265 = codec.compare(QStringLiteral("H265"), Qt::CaseInsensitive) == 0;
    const QString depayName = isH265 ? QStringLiteral("rtph265depay") : QStringLiteral("rtph264depay");
    const QString parseName = isH265 ? QStringLiteral("h265parse") : QStringLiteral("h264parse");
    const QString decoderName = PlatformGstElements::pickVideoDecoder(codec);

    if (decoderName.isEmpty()) {
        qCWarning(lcCamera) << "No usable decoder for codec" << codec << "on" << m_rtspUrl;
        setState(CameraState::Lost);
        return;
    }

    GstElement *depay = gst_element_factory_make(depayName.toUtf8().constData(), nullptr);
    GstElement *parse = gst_element_factory_make(parseName.toUtf8().constData(), nullptr);
    GstElement *decoder = gst_element_factory_make(decoderName.toUtf8().constData(), nullptr);
    GstElement *convert = gst_element_factory_make("videoconvert", nullptr);

    if (!depay || !parse || !decoder || !convert) {
        qCWarning(lcCamera) << "Failed to instantiate decode chain for" << m_rtspUrl;
        setState(CameraState::Lost);
        return;
    }

    gst_bin_add_many(GST_BIN(m_pipeline), depay, parse, decoder, convert, nullptr);
    if (!gst_element_link_many(depay, parse, decoder, convert, m_appsink, nullptr)) {
        qCWarning(lcCamera) << "Failed to link decode chain for" << m_rtspUrl;
        setState(CameraState::Lost);
        return;
    }

    gst_element_sync_state_with_parent(depay);
    gst_element_sync_state_with_parent(parse);
    gst_element_sync_state_with_parent(decoder);
    gst_element_sync_state_with_parent(convert);

    GstPad *sinkPad = gst_element_get_static_pad(depay, "sink");
    gst_pad_link(pad, sinkPad);
    gst_object_unref(sinkPad);

    m_dynamicElements = { depay, parse, decoder, convert };
}

int RtspStreamPipeline::newSampleCallback(GstElement *sink, void *userData)
{
    auto *self = static_cast<RtspStreamPipeline *>(userData);
    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample)
        return static_cast<int>(GST_FLOW_ERROR);

    self->m_videoItem->pushSample(sample); // takes ownership

    if (!self->m_seenFirstSample) {
        self->m_seenFirstSample = true;
        QMetaObject::invokeMethod(self, &RtspStreamPipeline::handleFirstSample, Qt::QueuedConnection);
    }
    return static_cast<int>(GST_FLOW_OK);
}

void RtspStreamPipeline::handleFirstSample()
{
    setState(CameraState::Live);
}
