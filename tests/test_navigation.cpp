// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "navigation/NavMath.h"
#include "navigation/NavigationController.h"

#include <QSignalSpy>
#include <QTest>

namespace {

// SPEC §8's own worked example: 10 cameras, 4 columns.
//   1  2  3  4
//   5  6  7  8
//   9 10  .  .
QStringList tenCameraIds()
{
    QStringList ids;
    for (int i = 1; i <= 10; ++i)
        ids << QStringLiteral("cam%1").arg(i);
    return ids;
}

} // namespace

class TestNavigation : public QObject
{
    Q_OBJECT

private slots:
    // NavMath -- pure function worked examples (SPEC §10/§12)
    void cyclicIndex_linearAdjacent();
    void cyclicIndex_wrapsAtEnds();
    void verticalMove_withinFullRows();
    void verticalMove_missingCellInSameColumnStaysPut();
    void verticalMove_noOpAtGridEdges();
    void zoomedPan_centerPivotScalesTowardCenter();
    void zoomedPan_offCenterPivotKeepsContentPointFixed();
    void clampPan_forcesZeroAtZoomOne();

    // NavigationController -- mosaic navigation (SPEC §10)
    void mosaic_rightAndLeftAreLinearNotRowCyclic();
    void mosaic_upDownStaysInColumn();

    // NavigationController -- fullscreen camera switching (SPEC §12)
    void fullscreen_leftRightCyclicAtZoomOne();
    void fullscreen_leftRightPanAtZoomAboveOne();

    // NavigationController -- zoom/reset (SPEC §14/§17/§18)
    void resetZoom_returnsToOneAndClearsPan();
    void afterZoomReset_leftRightSwitchesCameraAgain();
    void backWhileZoomed_resetsZoomInsteadOfLeavingFullscreen();

    // NavigationController -- shortcuts (SPEC §17/§23, acceptance #22)
    void cameraZeroIsAlwaysShortcutNeverZoomReset();
    void undefinedShortcutIsNoOp();

    // NavigationController -- diagnostics toggle is independent
    void toggleDiagnosticsIndependentOfViewState();
};

void TestNavigation::cyclicIndex_linearAdjacent()
{
    QCOMPARE(NavMath::cyclicIndex(3, 12, +1), 4); // 4 -> Right -> 5 (0-indexed: 3->4)
    QCOMPARE(NavMath::cyclicIndex(4, 12, -1), 3); // 5 -> Left -> 4
    QCOMPARE(NavMath::cyclicIndex(7, 12, +1), 8); // 8 -> Right -> 9
    QCOMPARE(NavMath::cyclicIndex(8, 12, -1), 7); // 9 -> Left -> 8
}

void TestNavigation::cyclicIndex_wrapsAtEnds()
{
    QCOMPARE(NavMath::cyclicIndex(0, 12, -1), 11); // first -> Left -> last
    QCOMPARE(NavMath::cyclicIndex(11, 12, +1), 0); // last -> Right -> first
}

void TestNavigation::verticalMove_withinFullRows()
{
    // row0 col2 (camera 3, index 2) -> Down -> row1 col2 (camera 7, index 6)
    QCOMPARE(NavMath::verticalMove(2, 4, 10, +1), 6);
    QCOMPARE(NavMath::verticalMove(6, 4, 10, -1), 2);
}

void TestNavigation::verticalMove_missingCellInSameColumnStaysPut()
{
    // row1 col3 (camera 8, index 7) -> Down -> row2 has no col3 cell at all
    // (only cameras 9/10 in col0/col1) -> stays at camera 8, does NOT jump
    // sideways into col0/col1 just because a camera exists there.
    QCOMPARE(NavMath::verticalMove(7, 4, 10, +1), 7);
    // row1 col2 (camera 7, index 6) -> Down -> row2 has no col2 either -> stays put.
    QCOMPARE(NavMath::verticalMove(6, 4, 10, +1), 6);
    // row2 col1 (camera 10, index 9) -> Up -> row1 col1 (camera 6, index 5)
    QCOMPARE(NavMath::verticalMove(9, 4, 10, -1), 5);
}

void TestNavigation::verticalMove_noOpAtGridEdges()
{
    QCOMPARE(NavMath::verticalMove(1, 4, 10, -1), 1); // top row, Up -> no-op
    QCOMPARE(NavMath::verticalMove(9, 4, 10, +1), 9); // no row below row2 -> no-op
}

void TestNavigation::zoomedPan_centerPivotScalesTowardCenter()
{
    const QPointF center(640, 360);
    // Pivot == center (CEC/keyboard zoom): existing pan scales proportionally.
    const QPointF result = NavMath::zoomedPan(QPointF(100, 50), 1.5, 3.0, center, center);
    QCOMPARE(result, QPointF(200, 100)); // k = 2.0
}

void TestNavigation::zoomedPan_offCenterPivotKeepsContentPointFixed()
{
    // SPEC §16: mouse wheel zoom-to-cursor -- the content pixel under a
    // genuinely off-center pivot (not the viewport center) must stay
    // under that exact screen position after the zoom step. This is the
    // actual "zoom to cursor" claim; the center-pivot test above alone
    // can't distinguish correct zoom-to-cursor math from simple zoom-to-
    // center math, since a center pivot can't tell the two apart.
    const QPointF center(640, 360);
    const QPointF cursor(800, 200); // deliberately off-center
    const QPointF oldPan(30, -10); // also start from a non-zero pan
    const qreal oldZoom = 1.5;
    const qreal newZoom = 3.0;

    const QPointF newPan = NavMath::zoomedPan(oldPan, oldZoom, newZoom, cursor, center);

    // Model (matches Fullscreen.qml's transformRoot): screen position of a
    // content point = center + pan + (contentOffsetFromCenter * zoom).
    const QPointF contentOffset = (cursor - center - oldPan) / oldZoom;
    const QPointF screenPosAfter = center + newPan + contentOffset * newZoom;
    QVERIFY(qFuzzyCompare(screenPosAfter.x() + 1.0, cursor.x() + 1.0)); // +1 guards against comparing near-zero
    QVERIFY(qFuzzyCompare(screenPosAfter.y() + 1.0, cursor.y() + 1.0)); // values, per qFuzzyCompare's own caveat
}

void TestNavigation::clampPan_forcesZeroAtZoomOne()
{
    const QPointF result = NavMath::clampPan(QPointF(500, 500), 1.0, QSizeF(1280, 720));
    QCOMPARE(result, QPointF(0, 0));
}

void TestNavigation::mosaic_rightAndLeftAreLinearNotRowCyclic()
{
    NavigationController nav(tenCameraIds(), 4);
    // Start at index 3 (camera 4) -> Right -> index 4 (camera 5), matching
    // SPEC §10's worked example exactly (not row-cyclic to index 0).
    nav.selectMosaicTile(3);
    nav.handleInputAction(InputAction::Back); // selectMosaicTile also enters fullscreen; return to mosaic
    QCOMPARE(nav.focusedMosaicIndex(), 3);

    nav.handleInputAction(InputAction::Right);
    QCOMPARE(nav.focusedMosaicIndex(), 4);
    nav.handleInputAction(InputAction::Left);
    QCOMPARE(nav.focusedMosaicIndex(), 3);
}

void TestNavigation::mosaic_upDownStaysInColumn()
{
    NavigationController nav(tenCameraIds(), 4);
    nav.selectMosaicTile(1); // camera 2
    nav.handleInputAction(InputAction::Back);
    nav.handleInputAction(InputAction::Down);
    QCOMPARE(nav.focusedMosaicIndex(), 5); // camera 6, same column
}

void TestNavigation::fullscreen_leftRightCyclicAtZoomOne()
{
    NavigationController nav(tenCameraIds(), 4);
    QSignalSpy activated(&nav, &NavigationController::fullscreenCameraActivated);

    nav.selectMosaicTile(0);
    QCOMPARE(nav.viewMode(), NavigationController::ViewMode::Fullscreen);
    QCOMPARE(nav.fullscreenIndex(), 0);

    nav.handleInputAction(InputAction::Left); // first -> Left -> last (cyclic, SPEC §12)
    QCOMPARE(nav.fullscreenIndex(), 9);

    nav.handleInputAction(InputAction::Right);
    QCOMPARE(nav.fullscreenIndex(), 0);
    QVERIFY(activated.count() >= 3); // initial entry + two switches
}

void TestNavigation::fullscreen_leftRightPanAtZoomAboveOne()
{
    NavigationController nav(tenCameraIds(), 4);
    nav.setViewportSize(QSizeF(1280, 720));
    nav.selectMosaicTile(4);
    nav.handleInputAction(InputAction::ZoomIn); // zoom > 1.0

    const int indexBefore = nav.fullscreenIndex();
    nav.handleInputAction(InputAction::Right); // must pan, not switch camera
    QCOMPARE(nav.fullscreenIndex(), indexBefore);
    QVERIFY(nav.pan() != QPointF(0, 0));

    nav.handleInputAction(InputAction::Up);
    nav.handleInputAction(InputAction::Down);
    nav.handleInputAction(InputAction::Left);
    QCOMPARE(nav.fullscreenIndex(), indexBefore); // still no camera switch
}

void TestNavigation::resetZoom_returnsToOneAndClearsPan()
{
    NavigationController nav(tenCameraIds(), 4);
    nav.setViewportSize(QSizeF(1280, 720));
    nav.selectMosaicTile(0);
    nav.handleInputAction(InputAction::ZoomIn);
    nav.handleInputAction(InputAction::Right); // pan away from (0,0)
    QVERIFY(nav.zoom() > 1.0);

    nav.handleInputAction(InputAction::ResetZoom);
    QCOMPARE(nav.zoom(), 1.0);
    QCOMPARE(nav.pan(), QPointF(0, 0));
}

void TestNavigation::backWhileZoomed_resetsZoomInsteadOfLeavingFullscreen()
{
    // Regression test: on real hardware, CEC's Back button maps to
    // InputAction::Back (not ResetZoom, as the keyboard's Escape does),
    // and used to drop straight to mosaic instead of resetting zoom first.
    NavigationController nav(tenCameraIds(), 4);
    nav.setViewportSize(QSizeF(1280, 720));
    nav.selectMosaicTile(0);
    nav.handleInputAction(InputAction::ZoomIn);
    nav.handleInputAction(InputAction::Right); // pan away from (0,0)
    QVERIFY(nav.zoom() > 1.0);

    nav.handleInputAction(InputAction::Back);
    QCOMPARE(nav.zoom(), 1.0);
    QCOMPARE(nav.pan(), QPointF(0, 0));
    QCOMPARE(nav.viewMode(), NavigationController::ViewMode::Fullscreen);

    nav.handleInputAction(InputAction::Back); // second Back now leaves fullscreen
    QCOMPARE(nav.viewMode(), NavigationController::ViewMode::Mosaic);
}

void TestNavigation::afterZoomReset_leftRightSwitchesCameraAgain()
{
    NavigationController nav(tenCameraIds(), 4);
    nav.setViewportSize(QSizeF(1280, 720));
    nav.selectMosaicTile(0);
    nav.handleInputAction(InputAction::ZoomIn);
    nav.handleInputAction(InputAction::ResetZoom);

    nav.handleInputAction(InputAction::Right);
    QCOMPARE(nav.fullscreenIndex(), 1); // camera switch, not pan
}

void TestNavigation::cameraZeroIsAlwaysShortcutNeverZoomReset()
{
    NavigationController nav(tenCameraIds(), 4);
    QHash<int, int> shortcuts;
    shortcuts[0] = 9; // camera 10 has shortcut 0
    nav.setShortcutMap(shortcuts);

    nav.selectMosaicTile(0);
    nav.handleInputAction(InputAction::ZoomIn); // zoom > 1.0, so if 0 were
                                                 // ever a reset it would fire here
    nav.handleInputAction(InputAction::Camera0);
    QCOMPARE(nav.fullscreenIndex(), 9);
    QCOMPARE(nav.zoom(), 1.0); // jumping to a different camera resets zoom
                               // as a side effect, but via the shortcut
                               // path, never via ResetZoom semantics
}

void TestNavigation::undefinedShortcutIsNoOp()
{
    NavigationController nav(tenCameraIds(), 4);
    QHash<int, int> shortcuts;
    shortcuts[5] = 4;
    nav.setShortcutMap(shortcuts); // digit 7 intentionally undefined
    nav.selectMosaicTile(0);
    nav.handleInputAction(InputAction::Back);

    nav.handleInputAction(InputAction::Camera7);
    QCOMPARE(nav.viewMode(), NavigationController::ViewMode::Mosaic); // no-op
}

void TestNavigation::toggleDiagnosticsIndependentOfViewState()
{
    NavigationController nav(tenCameraIds(), 4);
    QVERIFY(!nav.diagnosticsVisible());
    nav.handleInputAction(InputAction::ToggleDiagnostics);
    QVERIFY(nav.diagnosticsVisible());

    nav.selectMosaicTile(0);
    nav.handleInputAction(InputAction::ZoomIn);
    QVERIFY(nav.diagnosticsVisible()); // untouched by view/zoom changes

    nav.handleInputAction(InputAction::ToggleDiagnostics);
    QVERIFY(!nav.diagnosticsVisible());
}

QTEST_APPLESS_MAIN(TestNavigation)
#include "test_navigation.moc"
