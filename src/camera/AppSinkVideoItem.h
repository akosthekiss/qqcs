// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickItem>

typedef struct _GstSample GstSample;

// Portable (non-GL) video sink target: appsink delivers RGBA GstSamples on a
// GStreamer streaming thread via pushSample(); updatePaintNode() (Qt Quick
// render thread) uploads the latest one as a texture. This is the guaranteed
// fallback path used whenever qml6glsink isn't available (see
// PlatformGstElements::qmlGlSinkAvailable()).
class AppSinkVideoItem : public QQuickItem
{
    Q_OBJECT

public:
    // SPEC §9/§11: Cover crops (mosaic default); Contain letterboxes
    // (fullscreen default at 1.0x). Zoom/pan (fullscreen >1.0x, SPEC
    // §14/§18) is applied on top of whichever mode is active by
    // NavigationController via an outer QML transform, not by this item.
    // Fill (SPEC §6.2) is the one explicit, user-opted-in exception to
    // "never distorted" -- stretches to the item's exact bounds.
    enum class FillMode { Cover, Contain, Fill };

    explicit AppSinkVideoItem(QQuickItem *parent = nullptr);
    ~AppSinkVideoItem() override;

    int nativeWidth() const { return m_nativeWidth; }
    int nativeHeight() const { return m_nativeHeight; }

    FillMode fillMode() const { return m_fillMode; }
    void setFillMode(FillMode mode);

    // Called from a GStreamer streaming thread. Takes ownership of sample
    // (unrefs it before returning) and must never touch Qt/QML objects
    // beyond the thread-safe operations below.
    void pushSample(GstSample *sample);

signals:
    void videoSizeChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    QMutex m_mutex;
    QImage m_pendingFrame;
    bool m_hasPendingFrame = false;
    int m_nativeWidth = 0;
    int m_nativeHeight = 0;
    FillMode m_fillMode = FillMode::Cover;
};
