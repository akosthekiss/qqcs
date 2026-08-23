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
#include <QSize>

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

    int nativeWidth() const { return m_nativeSize.width(); }
    int nativeHeight() const { return m_nativeSize.height(); }

    FillMode fillMode() const { return m_fillMode; }
    void setFillMode(FillMode mode);

    // The space this item actually has to render into -- set from QML to
    // the fullscreen view's own size (window size), independent of this
    // item's own (QQuickItem) width/height, which is itself DERIVED from
    // contentSize() below. Feeding the two through separate properties
    // avoids a circular size dependency.
    QSizeF availableSize() const { return m_availableSize; }
    void setAvailableSize(QSizeF size);

    // The actual on-screen size of the rendered video within
    // availableSize(), given the current fillMode and native resolution:
    // == availableSize() for Cover/Fill (always fill exactly), or the
    // letterboxed/pillarboxed rect's own size for Contain. QML resizes
    // this item's (and its zoom/pan transform's) bounds to exactly this,
    // so panning/zooming can never reveal a black bar that CONTAIN alone
    // would otherwise leave outside the video (SPEC §34's "ZOOM/COVER").
    QSizeF contentSize() const;

    // Called from a GStreamer streaming thread. Takes ownership of sample
    // (unrefs it before returning) and must never touch Qt/QML objects
    // beyond the thread-safe operations below.
    void pushSample(GstSample *sample);

signals:
    void videoSizeChanged();
    void availableSizeChanged();
    void contentSizeChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    QMutex m_mutex;
    QImage m_pendingFrame;
    bool m_hasPendingFrame = false;
    QSize m_nativeSize; // always written/read together (pushSample()/contentSize())
    FillMode m_fillMode = FillMode::Cover;
    QSizeF m_availableSize;
};
