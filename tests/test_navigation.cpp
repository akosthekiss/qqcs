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
    void verticalMove_lastRowGapsClampToNearest();
    void verticalMove_noOpAtGridEdges();
    void zoomedPan_centerPivotScalesTowardCenter();
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

void TestNavigation::verticalMove_lastRowGapsClampToNearest()
{
    // row1 col3 (camera 8, index 7) -> Down -> row2 has only col0/col1
    // (cameras 9/10) -> clamp to last existing camera (index 9, camera 10)
    QCOMPARE(NavMath::verticalMove(7, 4, 10, +1), 9);
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
