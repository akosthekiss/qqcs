// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "NavigationController.h"

#include "NavMath.h"

#include <algorithm>

NavigationController::NavigationController(QStringList cameraIds, int columns, QVector<qreal> zoomSteps,
                                             QObject *parent)
    : QObject(parent), m_cameraIds(std::move(cameraIds)), m_columns(columns), m_zoomSteps(std::move(zoomSteps))
{
}

void NavigationController::setShortcutMap(const QHash<int, int> &shortcutDigitToIndex)
{
    m_shortcutDigitToIndex = shortcutDigitToIndex;
}

void NavigationController::handleInputAction(InputAction action)
{
    // Camera shortcuts take priority over everything else, in every state
    // (SPEC §23) -- in particular Camera0 can never collide with
    // ResetZoom, since this branch returns before the switch below ever
    // sees InputAction::ResetZoom (acceptance criterion #22).
    const int digit = cameraShortcutDigit(action);
    if (digit >= 0) {
        const auto it = m_shortcutDigitToIndex.constFind(digit);
        if (it != m_shortcutDigitToIndex.constEnd())
            jumpToCameraFullscreen(it.value());
        return; // undefined shortcut -> no-op
    }

    switch (action) {
    case InputAction::Select:
        if (m_viewMode == ViewMode::Mosaic)
            enterFullscreenAt(m_focusedMosaicIndex);
        break;
    case InputAction::Back:
        if (m_viewMode == ViewMode::Fullscreen)
            exitToMosaic();
        break;
    case InputAction::Up:
    case InputAction::Down:
        if (m_viewMode == ViewMode::Mosaic) {
            m_focusedMosaicIndex = NavMath::verticalMove(m_focusedMosaicIndex, m_columns, m_cameraIds.size(),
                                                          action == InputAction::Up ? -1 : +1);
            emit focusedMosaicIndexChanged();
        } else if (zoom() > 1.0) {
            panByKeyStep(action); // SPEC §18: zoom>1.0 remaps Up/Down to pan
        }
        break;
    case InputAction::Left:
    case InputAction::Right: {
        const int dir = (action == InputAction::Left) ? -1 : +1;
        if (m_viewMode == ViewMode::Mosaic) {
            m_focusedMosaicIndex = NavMath::cyclicIndex(m_focusedMosaicIndex, m_cameraIds.size(), dir);
            emit focusedMosaicIndexChanged();
        } else if (zoom() == 1.0) {
            m_fullscreenIndex = NavMath::cyclicIndex(m_fullscreenIndex, m_cameraIds.size(), dir);
            emit fullscreenIndexChanged();
            if (m_fullscreenIndex >= 0 && m_fullscreenIndex < m_cameraIds.size())
                emit fullscreenCameraActivated(m_cameraIds.at(m_fullscreenIndex));
        } else {
            panByKeyStep(action); // SPEC §18: zoom>1.0 remaps Left/Right to pan
        }
        break;
    }
    case InputAction::ZoomIn:
        applyZoomStep(+1, viewportCenter()); // SPEC §15: CEC/keyboard zoom pivots on the center
        break;
    case InputAction::ZoomOut:
        applyZoomStep(-1, viewportCenter());
        break;
    case InputAction::ResetZoom:
        if (zoom() > 1.0) {
            setZoomIndex(0); // SPEC §17
            setPan({ 0, 0 });
        } else if (m_viewMode == ViewMode::Fullscreen) {
            exitToMosaic(); // Escape at zoom==1.0 behaves like Back
        }
        break;
    case InputAction::ToggleDiagnostics:
        m_diagnosticsVisible = !m_diagnosticsVisible;
        emit diagnosticsVisibleChanged();
        break;
    default:
        break;
    }
}

void NavigationController::handleWheelZoom(qreal steps, QPointF cursorPosInViewport)
{
    if (steps == 0)
        return;
    applyZoomStep(steps > 0 ? +1 : -1, cursorPosInViewport); // SPEC §16: zoom-to-cursor
}

void NavigationController::handlePanDragDelta(QPointF delta)
{
    if (zoom() <= 1.0)
        return;
    setPan(NavMath::clampPan(m_pan + delta, zoom(), m_viewportSize));
}

void NavigationController::selectMosaicTile(int index)
{
    if (index < 0 || index >= m_cameraIds.size())
        return;
    m_focusedMosaicIndex = index;
    emit focusedMosaicIndexChanged();
    enterFullscreenAt(index);
}

void NavigationController::setViewportSize(QSizeF size)
{
    m_viewportSize = size;
}

void NavigationController::enterFullscreenAt(int index)
{
    if (index < 0 || index >= m_cameraIds.size())
        return;
    m_viewMode = ViewMode::Fullscreen;
    m_fullscreenIndex = index;
    setZoomIndex(0);
    setPan({ 0, 0 });
    emit viewModeChanged();
    emit fullscreenIndexChanged();
    emit fullscreenCameraActivated(m_cameraIds.at(index));
}

void NavigationController::exitToMosaic()
{
    m_viewMode = ViewMode::Mosaic;
    setZoomIndex(0);
    setPan({ 0, 0 });
    emit viewModeChanged();
    emit fullscreenExited();
}

void NavigationController::jumpToCameraFullscreen(int index)
{
    if (index < 0 || index >= m_cameraIds.size())
        return;
    const bool sameCamera = (m_viewMode == ViewMode::Fullscreen && m_fullscreenIndex == index);
    m_viewMode = ViewMode::Fullscreen;
    m_fullscreenIndex = index;
    if (!sameCamera) {
        setZoomIndex(0);
        setPan({ 0, 0 });
    }
    emit viewModeChanged();
    emit fullscreenIndexChanged();
    emit fullscreenCameraActivated(m_cameraIds.at(index));
}

void NavigationController::applyZoomStep(int direction, QPointF pivot)
{
    const int newIndex = std::clamp(m_zoomIndex + direction, 0, static_cast<int>(m_zoomSteps.size()) - 1);
    if (newIndex == m_zoomIndex)
        return;
    const qreal oldZoom = zoom();
    const QPointF newPan = NavMath::zoomedPan(m_pan, oldZoom, m_zoomSteps.at(newIndex), pivot, viewportCenter());
    setZoomIndex(newIndex);
    if (m_zoomIndex == 0)
        setPan({ 0, 0 });
    else
        setPan(NavMath::clampPan(newPan, zoom(), m_viewportSize));
}

void NavigationController::setZoomIndex(int index)
{
    if (m_zoomIndex == index)
        return;
    m_zoomIndex = index;
    emit zoomChanged();
}

void NavigationController::setPan(QPointF pan)
{
    if (m_pan == pan)
        return;
    m_pan = pan;
    emit panChanged();
}

void NavigationController::panByKeyStep(InputAction direction)
{
    // Pan represents the content's own translation, not the viewport's, so
    // "reveal more of the right side" (Right) moves the content left.
    QPointF delta;
    switch (direction) {
    case InputAction::Up:
        delta = { 0, kKeyPanStep };
        break;
    case InputAction::Down:
        delta = { 0, -kKeyPanStep };
        break;
    case InputAction::Left:
        delta = { kKeyPanStep, 0 };
        break;
    case InputAction::Right:
        delta = { -kKeyPanStep, 0 };
        break;
    default:
        return;
    }
    setPan(NavMath::clampPan(m_pan + delta, zoom(), m_viewportSize));
}
