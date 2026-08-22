// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "camera/AppSinkVideoItem.h"
#include "camera/CameraManager.h"
#include "camera/StreamUrlPolicy.h"

#include <QQuickItem>
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
    void mosaicPrefersSubUrl();
    void mosaicVideoItemsExistPerCamera();
    void pipelinesNotStartedUntilStartCalled();
    void fillModesDefaultToCoverAndContain();
    void fillModesHonorConfigOverride();
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

void TestCameraManager::mosaicPrefersSubUrl()
{
    const auto cameras = makeConfig().cameras;
    QCOMPARE(StreamUrlPolicy::mosaicUrl(cameras[0]), QStringLiteral("rtsp://a/sub")); // has subUrl
    QCOMPARE(StreamUrlPolicy::mosaicUrl(cameras[1]), QStringLiteral("rtsp://b/main")); // no subUrl
    QCOMPARE(StreamUrlPolicy::fullscreenUrl(cameras[0]), QStringLiteral("rtsp://a/main"));
}

void TestCameraManager::mosaicVideoItemsExistPerCamera()
{
    CameraManager manager(makeConfig());
    QVERIFY(manager.mosaicVideoItem(QStringLiteral("front")) != nullptr);
    QVERIFY(manager.mosaicVideoItem(QStringLiteral("garden")) != nullptr);
    QVERIFY(manager.mosaicVideoItem(QStringLiteral("nope")) == nullptr);
    QVERIFY(manager.fullscreenVideoItem() == nullptr);

    manager.enterFullscreen(QStringLiteral("front"));
    QVERIFY(manager.fullscreenVideoItem() != nullptr);
}

void TestCameraManager::pipelinesNotStartedUntilStartCalled()
{
    // Constructing (and even entering fullscreen on) a CameraManager must
    // never trigger real network I/O on its own -- only start() does. This
    // is what keeps this whole test file fast and network-free using fake
    // rtsp:// URLs.
    CameraManager manager(makeConfig());
    QCOMPARE(manager.listModel()->data(manager.listModel()->index(0), CameraListModel::StateRole).toInt(),
              static_cast<int>(CameraState::Disconnected));
    manager.enterFullscreen(QStringLiteral("front"));
    QCOMPARE(manager.listModel()->data(manager.listModel()->index(0), CameraListModel::StateRole).toInt(),
              static_cast<int>(CameraState::Disconnected));
}

void TestCameraManager::fillModesDefaultToCoverAndContain()
{
    CameraManager manager(makeConfig()); // layout.mosaicFillMode/fullscreenFillMode left at their defaults

    QQuickItem mosaicContainer;
    manager.attachMosaicVideo(QStringLiteral("front"), &mosaicContainer);
    QCOMPARE(manager.mosaicVideoItem(QStringLiteral("front"))->fillMode(), AppSinkVideoItem::FillMode::Cover);

    manager.enterFullscreen(QStringLiteral("front"));
    QQuickItem fullscreenContainer;
    manager.attachFullscreenVideo(&fullscreenContainer);
    QCOMPARE(manager.fullscreenVideoItem()->fillMode(), AppSinkVideoItem::FillMode::Contain);
}

void TestCameraManager::fillModesHonorConfigOverride()
{
    // SPEC §6.2: "fill" is the one explicit, opt-in exception that may
    // distort the image -- CameraManager must actually apply it, not
    // silently keep the cover/contain defaults.
    AppConfig config = makeConfig();
    config.layout.mosaicFillMode = QStringLiteral("fill");
    config.layout.fullscreenFillMode = QStringLiteral("fill");
    CameraManager manager(std::move(config));

    QQuickItem mosaicContainer;
    manager.attachMosaicVideo(QStringLiteral("front"), &mosaicContainer);
    QCOMPARE(manager.mosaicVideoItem(QStringLiteral("front"))->fillMode(), AppSinkVideoItem::FillMode::Fill);

    manager.enterFullscreen(QStringLiteral("front"));
    QQuickItem fullscreenContainer;
    manager.attachFullscreenVideo(&fullscreenContainer);
    QCOMPARE(manager.fullscreenVideoItem()->fillMode(), AppSinkVideoItem::FillMode::Fill);
}

QTEST_APPLESS_MAIN(TestCameraManager)
#include "test_cameramanager.moc"
