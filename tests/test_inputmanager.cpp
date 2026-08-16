#include "input/InputManager.h"
#include "input/KeyboardMapping.h"

#include <QSignalSpy>
#include <QTest>
#include <Qt>

class TestInputManager : public QObject
{
    Q_OBJECT

private slots:
    void arrowKeysMapToDirections();
    void enterMapsToSelect();
    void escapeMapsToResetZoomNeverBack();
    void digitZeroMapsToCameraZeroNeverResetZoom();
    void allTenDigitsMapToCameraShortcuts();
    void plusMinusMapToZoom();
    void iMapsToToggleDiagnostics();
    void unmappedKeyIsNoOp();
    void inputManagerEmitsMappedAction();
};

void TestInputManager::arrowKeysMapToDirections()
{
    InputAction action;
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Up, action));
    QCOMPARE(action, InputAction::Up);
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Down, action));
    QCOMPARE(action, InputAction::Down);
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Left, action));
    QCOMPARE(action, InputAction::Left);
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Right, action));
    QCOMPARE(action, InputAction::Right);
}

void TestInputManager::enterMapsToSelect()
{
    InputAction action;
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Return, action));
    QCOMPARE(action, InputAction::Select);
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Enter, action));
    QCOMPARE(action, InputAction::Select);
}

void TestInputManager::escapeMapsToResetZoomNeverBack()
{
    // SPEC §17/§26: Escape is ResetZoom; the "or Back" behavior at
    // zoom==1.0 is NavigationController state logic, never a second
    // keyboard mapping -- there is no InputAction::Back mapping here at all.
    InputAction action;
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Escape, action));
    QCOMPARE(action, InputAction::ResetZoom);
}

void TestInputManager::digitZeroMapsToCameraZeroNeverResetZoom()
{
    // Acceptance criterion #22: 0 is always a camera shortcut.
    InputAction action;
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_0, action));
    QCOMPARE(action, InputAction::Camera0);
    QVERIFY(action != InputAction::ResetZoom);
}

void TestInputManager::allTenDigitsMapToCameraShortcuts()
{
    const InputAction expected[] = { InputAction::Camera0, InputAction::Camera1, InputAction::Camera2,
                                      InputAction::Camera3, InputAction::Camera4, InputAction::Camera5,
                                      InputAction::Camera6, InputAction::Camera7, InputAction::Camera8,
                                      InputAction::Camera9 };
    for (int digit = 0; digit <= 9; ++digit) {
        InputAction action;
        QVERIFY(KeyboardMapping::mapKey(Qt::Key_0 + digit, action));
        QCOMPARE(action, expected[digit]);
        QCOMPARE(cameraShortcutDigit(action), digit);
    }
}

void TestInputManager::plusMinusMapToZoom()
{
    InputAction action;
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Plus, action));
    QCOMPARE(action, InputAction::ZoomIn);
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Equal, action));
    QCOMPARE(action, InputAction::ZoomIn);
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_Minus, action));
    QCOMPARE(action, InputAction::ZoomOut);
}

void TestInputManager::iMapsToToggleDiagnostics()
{
    InputAction action;
    QVERIFY(KeyboardMapping::mapKey(Qt::Key_I, action));
    QCOMPARE(action, InputAction::ToggleDiagnostics);
}

void TestInputManager::unmappedKeyIsNoOp()
{
    InputAction action;
    QVERIFY(!KeyboardMapping::mapKey(Qt::Key_F1, action));
    QVERIFY(!KeyboardMapping::mapKey(Qt::Key_Tab, action));
}

void TestInputManager::inputManagerEmitsMappedAction()
{
    InputManager manager;
    QSignalSpy spy(&manager, &InputManager::actionTriggered);

    manager.handleKeyEvent(Qt::Key_Right);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<InputAction>(), InputAction::Right);

    manager.handleKeyEvent(Qt::Key_F1); // unmapped -> no signal
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(TestInputManager)
#include "test_inputmanager.moc"
