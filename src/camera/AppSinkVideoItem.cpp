#include "AppSinkVideoItem.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#include <QQuickWindow>
#include <QSGSimpleTextureNode>

AppSinkVideoItem::AppSinkVideoItem(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

AppSinkVideoItem::~AppSinkVideoItem() = default;

void AppSinkVideoItem::pushSample(GstSample *sample)
{
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    if (!buffer || !caps) {
        gst_sample_unref(sample);
        return;
    }

    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps)) {
        gst_sample_unref(sample);
        return;
    }

    GstVideoFrame frame;
    if (!gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return;
    }

    const int width = GST_VIDEO_FRAME_WIDTH(&frame);
    const int height = GST_VIDEO_FRAME_HEIGHT(&frame);
    const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
    const auto *data = static_cast<const uchar *>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));

    // .copy() detaches from the GstBuffer-backed memory before we unmap/unref it.
    QImage image = QImage(data, width, height, stride, QImage::Format_RGBA8888).copy();

    gst_video_frame_unmap(&frame);
    gst_sample_unref(sample);

    bool sizeChanged = false;
    {
        QMutexLocker locker(&m_mutex);
        m_pendingFrame = image;
        m_hasPendingFrame = true;
        sizeChanged = (m_nativeWidth != width || m_nativeHeight != height);
        m_nativeWidth = width;
        m_nativeHeight = height;
    }

    if (sizeChanged)
        QMetaObject::invokeMethod(this, &AppSinkVideoItem::videoSizeChanged, Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

QSGNode *AppSinkVideoItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QImage frame;
    bool hasFrame = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_hasPendingFrame) {
            frame = m_pendingFrame;
            hasFrame = true;
        }
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!hasFrame)
        return node;

    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
    }

    node->setTexture(window()->createTextureFromImage(frame));
    node->setRect(boundingRect());
    return node;
}
