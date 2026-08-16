#include "cec/CecMapping.h"

#include <QTest>

class TestCecMapping : public QObject
{
    Q_OBJECT

private slots:
    void directionsMapToNavigation();
    void selectAndBackMap();
    void colorButtonsMapToZoomAndDiagnostics();
    void yellowIsReservedAndNeverMaps();
    void allTenDigitsMapToCameraShortcuts();
};

void TestCecMapping::directionsMapToNavigation()
{
    InputAction action;
    QVERIFY(CecMapping::mapButton(CecButton::Up, action));
    QCOMPARE(action, InputAction::Up);
    QVERIFY(CecMapping::mapButton(CecButton::Down, action));
    QCOMPARE(action, InputAction::Down);
    QVERIFY(CecMapping::mapButton(CecButton::Left, action));
    QCOMPARE(action, InputAction::Left);
    QVERIFY(CecMapping::mapButton(CecButton::Right, action));
    QCOMPARE(action, InputAction::Right);
}

void TestCecMapping::selectAndBackMap()
{
    InputAction action;
    QVERIFY(CecMapping::mapButton(CecButton::Select, action));
    QCOMPARE(action, InputAction::Select);
    QVERIFY(CecMapping::mapButton(CecButton::Back, action));
    QCOMPARE(action, InputAction::Back);
}

void TestCecMapping::colorButtonsMapToZoomAndDiagnostics()
{
    // SPEC §15: Red -> Zoom+, Green -> Zoom-, Blue -> Diagnostics.
    InputAction action;
    QVERIFY(CecMapping::mapButton(CecButton::Red, action));
    QCOMPARE(action, InputAction::ZoomIn);
    QVERIFY(CecMapping::mapButton(CecButton::Green, action));
    QCOMPARE(action, InputAction::ZoomOut);
    QVERIFY(CecMapping::mapButton(CecButton::Blue, action));
    QCOMPARE(action, InputAction::ToggleDiagnostics);
}

void TestCecMapping::yellowIsReservedAndNeverMaps()
{
    InputAction action;
    QVERIFY(!CecMapping::mapButton(CecButton::Yellow, action));
}

void TestCecMapping::allTenDigitsMapToCameraShortcuts()
{
    const CecButton digits[] = { CecButton::Digit0, CecButton::Digit1, CecButton::Digit2, CecButton::Digit3,
                                  CecButton::Digit4, CecButton::Digit5, CecButton::Digit6, CecButton::Digit7,
                                  CecButton::Digit8, CecButton::Digit9 };
    for (int i = 0; i <= 9; ++i) {
        InputAction action;
        QVERIFY(CecMapping::mapButton(digits[i], action));
        QCOMPARE(cameraShortcutDigit(action), i);
    }
}

QTEST_APPLESS_MAIN(TestCecMapping)
#include "test_cecmapping.moc"
