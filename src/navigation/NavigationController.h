#pragma once

#include "input/InputAction.h"

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVector>

// Owns view/focus/zoom/pan state and decides what each InputAction means
// in the current state (SPEC §24: "NavigationController dönti el az action
// jelentését"). InputManager (a later milestone) always emits the same
// plain action regardless of app state -- all state-dependent remapping
// lives here, not there.
class NavigationController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ViewMode viewMode READ viewMode NOTIFY viewModeChanged)
    Q_PROPERTY(int focusedMosaicIndex READ focusedMosaicIndex NOTIFY focusedMosaicIndexChanged)
    Q_PROPERTY(int fullscreenIndex READ fullscreenIndex NOTIFY fullscreenIndexChanged)
    Q_PROPERTY(qreal zoom READ zoom NOTIFY zoomChanged)
    Q_PROPERTY(QPointF pan READ pan NOTIFY panChanged)
    Q_PROPERTY(bool diagnosticsVisible READ diagnosticsVisible NOTIFY diagnosticsVisibleChanged)
    Q_PROPERTY(bool isFullscreen READ isFullscreen NOTIFY viewModeChanged)

public:
    enum class ViewMode { Mosaic, Fullscreen };
    Q_ENUM(ViewMode)

    explicit NavigationController(QStringList cameraIds, int columns,
                                   QVector<qreal> zoomSteps = { 1.0, 1.5, 2.0, 3.0, 4.0 },
                                   QObject *parent = nullptr);

    // Built from AppConfig's CameraConfig::shortcut fields (digit -> index
    // in cameraIds). SPEC §23: undefined shortcuts are a no-op.
    void setShortcutMap(const QHash<int, int> &shortcutDigitToIndex);

    ViewMode viewMode() const { return m_viewMode; }
    bool isFullscreen() const { return m_viewMode == ViewMode::Fullscreen; }
    int focusedMosaicIndex() const { return m_focusedMosaicIndex; }
    int fullscreenIndex() const { return m_fullscreenIndex; }
    qreal zoom() const { return m_zoomSteps.value(m_zoomIndex, 1.0); }
    QPointF pan() const { return m_pan; }
    bool diagnosticsVisible() const { return m_diagnosticsVisible; }

public slots:
    void handleInputAction(InputAction action);
    void handleWheelZoom(qreal steps, QPointF cursorPosInViewport);
    void handlePanDragDelta(QPointF delta);
    void selectMosaicTile(int index);
    void setViewportSize(QSizeF size);

signals:
    void viewModeChanged();
    void focusedMosaicIndexChanged();
    void fullscreenIndexChanged();
    void zoomChanged();
    void panChanged();
    void diagnosticsVisibleChanged();
    void fullscreenCameraActivated(const QString &cameraId);
    void fullscreenExited();

private:
    void enterFullscreenAt(int index);
    void exitToMosaic();
    void jumpToCameraFullscreen(int index);
    void applyZoomStep(int direction, QPointF pivot);
    void setZoomIndex(int index);
    void setPan(QPointF pan);
    void panByKeyStep(InputAction direction);
    QPointF viewportCenter() const { return { m_viewportSize.width() / 2.0, m_viewportSize.height() / 2.0 }; }

    QStringList m_cameraIds;
    int m_columns;
    QVector<qreal> m_zoomSteps;
    QHash<int, int> m_shortcutDigitToIndex;

    ViewMode m_viewMode = ViewMode::Mosaic;
    int m_focusedMosaicIndex = 0;
    int m_fullscreenIndex = 0;
    int m_zoomIndex = 0;
    QPointF m_pan{ 0, 0 };
    bool m_diagnosticsVisible = false;
    QSizeF m_viewportSize{ 1280, 720 };

    static constexpr qreal kKeyPanStep = 60.0;
};
