// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "AppSinkVideoItem.h"

#include "VideoFillMath.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#include <QQuickWindow>
#include <QSGSimpleTextureNode>

AppSinkVideoItem::AppSinkVideoItem(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

AppSinkVideoItem::~AppSinkVideoItem() = default;

void AppSinkVideoItem::setFillMode(FillMode mode)
{
    if (m_fillMode == mode)
        return;
    m_fillMode = mode;
    update();
}

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
            m_hasPendingFrame = false;
        }
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        if (!hasFrame)
            return nullptr;
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
    }

    if (hasFrame)
        node->setTexture(window()->createTextureFromImage(frame));

    const QSGTexture *texture = node->texture();
    const QSizeF texSize = texture ? texture->textureSize() : QSizeF();
    const qreal itemW = width();
    const qreal itemH = height();
    if (texSize.width() <= 0 || texSize.height() <= 0 || itemW <= 0 || itemH <= 0) {
        node->setSourceRect(QRectF(QPointF(0, 0), texSize));
        node->setRect(boundingRect());
        return node;
    }

    const QSizeF itemSize(itemW, itemH);
    switch (m_fillMode) {
    case FillMode::Cover:
        node->setSourceRect(VideoFillMath::coverSourceRect(texSize, itemSize));
        node->setRect(boundingRect());
        break;
    case FillMode::Contain:
        node->setSourceRect(QRectF(QPointF(0, 0), texSize));
        node->setRect(VideoFillMath::containDestRect(texSize, itemSize));
        break;
    case FillMode::Fill:
        // Full source, full destination -- no crop, no letterbox, and
        // therefore no aspect-ratio preservation either. The one mode
        // that can distort (SPEC §6.2's explicit, opt-in exception).
        node->setSourceRect(QRectF(QPointF(0, 0), texSize));
        node->setRect(boundingRect());
        break;
    }

    return node;
}
