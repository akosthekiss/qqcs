// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "camera/VideoFillMath.h"

#include <QTest>

// SPEC §9/§34: video must never be distorted. Cover crops (mosaic);
// Contain letterboxes (fullscreen at 1.0x). No QQuickWindow/QSGTexture
// needed -- this is the pure math AppSinkVideoItem::updatePaintNode calls.
class TestVideoFillMath : public QObject
{
    Q_OBJECT

private slots:
    void coverCropsWidthWhenVideoWiderThanItem();
    void coverCropsHeightWhenVideoTallerThanItem();
    void coverNoCropWhenAspectMatches();
    void containPillarboxesWhenItemWiderThanVideo();
    void containLetterboxesWhenItemTallerThanVideo();
    void containNoBarsWhenAspectMatches();
    void degenerateSizesDoNotCrashOrProduceNaN();
};

void TestVideoFillMath::coverCropsWidthWhenVideoWiderThanItem()
{
    // 16:9 video into a narrow, tall tile -> crop left/right, keep full height.
    const QRectF src = VideoFillMath::coverSourceRect(QSizeF(1920, 1080), QSizeF(300, 720));
    QCOMPARE(src.height(), 1080.0);
    QVERIFY(src.width() < 1920.0);
    QCOMPARE(src.x(), (1920.0 - src.width()) / 2.0); // centered crop (SPEC §9)
}

void TestVideoFillMath::coverCropsHeightWhenVideoTallerThanItem()
{
    // Same video into a wide, short tile -> crop top/bottom, keep full width.
    const QRectF src = VideoFillMath::coverSourceRect(QSizeF(1920, 1080), QSizeF(1920, 300));
    QCOMPARE(src.width(), 1920.0);
    QVERIFY(src.height() < 1080.0);
    QCOMPARE(src.y(), (1080.0 - src.height()) / 2.0);
}

void TestVideoFillMath::coverNoCropWhenAspectMatches()
{
    const QRectF src = VideoFillMath::coverSourceRect(QSizeF(1920, 1080), QSizeF(1280, 720));
    QCOMPARE(src, QRectF(0, 0, 1920, 1080));
}

void TestVideoFillMath::containPillarboxesWhenItemWiderThanVideo()
{
    // 16:9 video into a wider viewport -> pillarbox (bars on the sides),
    // full height visible, nothing cropped (SPEC §11: full video visible).
    const QRectF dest = VideoFillMath::containDestRect(QSizeF(1920, 1080), QSizeF(2560, 1080));
    QCOMPARE(dest.height(), 1080.0);
    QVERIFY(dest.width() < 2560.0);
    QCOMPARE(dest.x(), (2560.0 - dest.width()) / 2.0);
}

void TestVideoFillMath::containLetterboxesWhenItemTallerThanVideo()
{
    const QRectF dest = VideoFillMath::containDestRect(QSizeF(1920, 1080), QSizeF(1920, 2000));
    QCOMPARE(dest.width(), 1920.0);
    QVERIFY(dest.height() < 2000.0);
    QCOMPARE(dest.y(), (2000.0 - dest.height()) / 2.0);
}

void TestVideoFillMath::containNoBarsWhenAspectMatches()
{
    const QRectF dest = VideoFillMath::containDestRect(QSizeF(1920, 1080), QSizeF(1280, 720));
    QCOMPARE(dest, QRectF(0, 0, 1280, 720));
}

void TestVideoFillMath::degenerateSizesDoNotCrashOrProduceNaN()
{
    const QRectF src = VideoFillMath::coverSourceRect(QSizeF(0, 0), QSizeF(1280, 720));
    QVERIFY(!std::isnan(src.width()) && !std::isnan(src.height()));

    const QRectF dest = VideoFillMath::containDestRect(QSizeF(1920, 1080), QSizeF(0, 0));
    QVERIFY(!std::isnan(dest.width()) && !std::isnan(dest.height()));
}

QTEST_APPLESS_MAIN(TestVideoFillMath)
#include "test_videofillmath.moc"
