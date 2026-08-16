#include "camera/CameraManager.h"

#include <QSignalSpy>
#include <QTest>

namespace {

AppConfig makeConfig()
{
    AppConfig config;
    config.layout.columns = 4;
    CameraConfig front;
    front.id = QStringLiteral("front");
    front.name = QStringLiteral("Front");
    front.shortcut = 1;
    front.mainUrl = QStringLiteral("rtsp://a/main");
    front.subUrl = QStringLiteral("rtsp://a/sub");
    CameraConfig garden;
    garden.id = QStringLiteral("garden");
    garden.mainUrl = QStringLiteral("rtsp://b/main");
    config.cameras = { front, garden };
    return config;
}

} // namespace

class TestCameraManager : public QObject
{
    Q_OBJECT

private slots:
    void listModelReflectsConfig();
    void defaultStateIsDisconnected();
    void fullscreenLifecycle();
    void focusHasNoSideEffects();
};

void TestCameraManager::listModelReflectsConfig()
{
    CameraManager manager(makeConfig());
    auto *model = manager.listModel();
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->data(model->index(0), CameraListModel::IdRole).toString(), QStringLiteral("front"));
    QCOMPARE(model->data(model->index(1), CameraListModel::IdRole).toString(), QStringLiteral("garden"));
    QCOMPARE(model->indexOfId(QStringLiteral("garden")), 1);
    QCOMPARE(model->indexOfId(QStringLiteral("nope")), -1);
}

void TestCameraManager::defaultStateIsDisconnected()
{
    CameraManager manager(makeConfig());
    auto *model = manager.listModel();
    QCOMPARE(model->data(model->index(0), CameraListModel::StateRole).toInt(),
              static_cast<int>(CameraState::Disconnected));
}

void TestCameraManager::fullscreenLifecycle()
{
    CameraManager manager(makeConfig());
    QSignalSpy spy(&manager, &CameraManager::fullscreenIdChanged);

    QVERIFY(manager.currentFullscreenId().isEmpty());
    manager.enterFullscreen(QStringLiteral("front"));
    QCOMPARE(manager.currentFullscreenId(), QStringLiteral("front"));
    QCOMPARE(spy.count(), 1);

    manager.switchFullscreenCamera(QStringLiteral("garden"));
    QCOMPARE(manager.currentFullscreenId(), QStringLiteral("garden"));
    QCOMPARE(spy.count(), 2);

    manager.exitFullscreen();
    QVERIFY(manager.currentFullscreenId().isEmpty());
    QCOMPARE(spy.count(), 3);
}

void TestCameraManager::focusHasNoSideEffects()
{
    CameraManager manager(makeConfig());
    QSignalSpy spy(&manager, &CameraManager::fullscreenIdChanged);
    manager.focus(QStringLiteral("front"));
    QCOMPARE(spy.count(), 0);
    QVERIFY(manager.currentFullscreenId().isEmpty());
}

QTEST_APPLESS_MAIN(TestCameraManager)
#include "test_cameramanager.moc"
