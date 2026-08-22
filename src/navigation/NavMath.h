// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include <QPointF>
#include <QSizeF>

#include <algorithm>

// Pure, zero-dependency math -- no QObject, no event loop -- so every
// worked example in SPEC.md §10/§12/§14 is directly assertable in a plain
// unit test.
namespace NavMath {

// Mosaic Left/Right (SPEC §10) and fullscreen zoom==1.0 Left/Right (SPEC
// §12) are the same cyclic walk over the full YAML-order camera list.
// dir is -1 (Left) or +1 (Right).
inline int cyclicIndex(int current, int count, int dir)
{
    if (count <= 0)
        return current;
    return ((current + dir) % count + count) % count;
}

// Mosaic Up/Down (SPEC §10): same-column row walk. If the target row has
// no cell in the *same* column -- the trailing, partially-empty last row
// -- the move is a no-op: focus stays put at the bottom of its current
// column, rather than jumping sideways into a different column just
// because some camera happens to exist further along that row. Row-major
// fill means that once a column is missing at some row, it's missing at
// every row after that too, so "stay put" is always the right answer,
// never "snap to a lower cell in the same column instead".
// dir is -1 (Up) or +1 (Down).
inline int verticalMove(int current, int columns, int count, int dir)
{
    if (columns <= 0 || count <= 0)
        return current;
    const int row = current / columns;
    const int col = current % columns;
    const int targetRow = row + dir;
    if (targetRow < 0)
        return current;
    const int targetIndex = targetRow * columns + col;
    if (targetIndex >= count)
        return current;
    return targetIndex;
}

// Zoom-to-point pivot math (SPEC §15/§16): the content pixel under `pivot`
// must stay under `pivot` after the zoom step. CEC/keyboard zoom passes
// viewportCenter as the pivot (zoom-to-center); mouse wheel passes the
// cursor position (zoom-to-cursor). Derivation: solving
// pivot = origin(oldZoom, oldPan) + v * scale(oldZoom) and
// pivot = origin(newZoom, newPan) + v * scale(newZoom) for the same
// content point v collapses to this, independent of content/viewport size.
inline QPointF zoomedPan(QPointF oldPan, qreal oldZoom, qreal newZoom, QPointF pivot, QPointF viewportCenter)
{
    if (oldZoom <= 0)
        return oldPan;
    const qreal k = newZoom / oldZoom;
    const QPointF u = pivot - viewportCenter;
    return k * oldPan + (1.0 - k) * u;
}

// Keeps panned content from going out of its valid range. `contentSize` is
// the actual on-screen size of the video at zoom==1.0 (AppSinkVideoItem::
// contentSize(), forwarded via CameraManager::fullscreenContentSize()) --
// equal to `viewport` for Cover/Fill (no letterbox at all), but smaller
// than it in the letterboxed axis for Contain. Scaling that by `zoom` and
// comparing against `viewport` (not `contentSize` again) is what
// implements SPEC §34's "ZOOM/COVER": below the zoom level where
// contentSize*zoom reaches viewport size in a given axis, max pan in that
// axis is forced to 0 -- there is no pan position, however small, that
// wouldn't reveal a black bar the CONTAIN letterbox already unavoidably
// has at that zoom -- so it collapses to the exact same "no pan below
// that point" behavior a tight clamp alone could give, without pretending
// pan could ever fully hide a bar zoom hasn't yet grown past. Once past
// it, this reduces to the original Cover-style formula
// (viewport*(zoom-1)/2) exactly, since contentSize==viewport there.
inline QPointF clampPan(QPointF pan, qreal zoom, QSizeF contentSize, QSizeF viewport)
{
    const qreal maxX = std::max(0.0, contentSize.width() * zoom - viewport.width()) / 2.0;
    const qreal maxY = std::max(0.0, contentSize.height() * zoom - viewport.height()) / 2.0;
    return { std::clamp(pan.x(), -maxX, maxX), std::clamp(pan.y(), -maxY, maxY) };
}

} // namespace NavMath
