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
    explicit AppSinkVideoItem(QQuickItem *parent = nullptr);
    ~AppSinkVideoItem() override;

    int nativeWidth() const { return m_nativeWidth; }
    int nativeHeight() const { return m_nativeHeight; }

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
};
