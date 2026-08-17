#pragma once

#include <QRectF>
#include <QSizeF>

// Pure aspect-ratio math extracted from AppSinkVideoItem::updatePaintNode
// so it's directly unit-testable without a QQuickWindow/QSGTexture. SPEC
// §9/§34: video must never be distorted -- Cover crops (mosaic), Contain
// letterboxes (fullscreen at 1.0x).
namespace VideoFillMath {

// The sub-rect of the source texture to sample from, in texture pixel
// coordinates, so that texSize's aspect ratio is cropped down to match
// itemSize's -- centered, per SPEC §9's requirement that cropping default
// to a centered crop.
inline QRectF coverSourceRect(QSizeF texSize, QSizeF itemSize)
{
    if (texSize.width() <= 0 || texSize.height() <= 0 || itemSize.width() <= 0 || itemSize.height() <= 0)
        return QRectF(QPointF(0, 0), texSize);

    const qreal videoAspect = texSize.width() / texSize.height();
    const qreal itemAspect = itemSize.width() / itemSize.height();

    QRectF src(QPointF(0, 0), texSize);
    if (videoAspect > itemAspect) {
        const qreal wantedWidth = texSize.height() * itemAspect;
        src.setX((texSize.width() - wantedWidth) / 2.0);
        src.setWidth(wantedWidth);
    } else if (videoAspect < itemAspect) {
        const qreal wantedHeight = texSize.width() / itemAspect;
        src.setY((texSize.height() - wantedHeight) / 2.0);
        src.setHeight(wantedHeight);
    }
    return src;
}

// The sub-rect of the item to draw the (uncropped) texture into, so that
// texSize's aspect ratio is preserved and letterboxed/pillarboxed within
// itemSize -- centered.
inline QRectF containDestRect(QSizeF texSize, QSizeF itemSize)
{
    if (texSize.width() <= 0 || texSize.height() <= 0 || itemSize.width() <= 0 || itemSize.height() <= 0)
        return QRectF(QPointF(0, 0), itemSize);

    const qreal videoAspect = texSize.width() / texSize.height();
    const qreal itemAspect = itemSize.width() / itemSize.height();

    if (itemAspect > videoAspect) {
        const qreal destWidth = itemSize.height() * videoAspect;
        return QRectF((itemSize.width() - destWidth) / 2.0, 0, destWidth, itemSize.height());
    }
    const qreal destHeight = itemSize.width() / videoAspect;
    return QRectF(0, (itemSize.height() - destHeight) / 2.0, itemSize.width(), destHeight);
}

} // namespace VideoFillMath
