// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "camera/ReconnectScheduler.h"

#include <QSignalSpy>
#include <QTest>

// Headless, no GStreamer/network involved -- exercises the real QTimer/
// QDeadlineTimer-driven scheduler exactly as RtspStreamPipeline uses it.
class TestReconnectScheduler : public QObject
{
    Q_OBJECT

private slots:
    void backoffSequenceAdvancesOnRepeatedFailure();
    void succeedingResetsStepButNotCount();
    void countdownComesFromRealDeadlineTimer();
    void attemptStartedHidesCountdown();
};

void TestReconnectScheduler::backoffSequenceAdvancesOnRepeatedFailure()
{
    // SPEC §33: 1s, 2s, 5s, 10s, 30s, then repeating 30s.
    ReconnectScheduler scheduler;
    const QList<int> expected = { 1, 2, 5, 10, 30, 30, 30 };
    for (int expectedBackoff : expected) {
        scheduler.onConnectionLost();
        QCOMPARE(scheduler.backoffSeconds(), expectedBackoff);
    }
}

void TestReconnectScheduler::succeedingResetsStepButNotCount()
{
    ReconnectScheduler scheduler;
    scheduler.onConnectionLost(); // step -> 1s, advances to next=2s
    scheduler.onConnectionLost(); // step -> 2s, advances to next=5s
    scheduler.onAttemptStarted();
    QCOMPARE(scheduler.reconnectCount(), 1);

    scheduler.onConnectSucceeded();
    QCOMPARE(scheduler.isScheduled(), false);

    scheduler.onConnectionLost(); // fresh loss after a success -> back to 1s
    QCOMPARE(scheduler.backoffSeconds(), 1);

    // reconnectCount is cumulative and must NOT be reset by success.
    scheduler.onAttemptStarted();
    QCOMPARE(scheduler.reconnectCount(), 2);
}

void TestReconnectScheduler::countdownComesFromRealDeadlineTimer()
{
    ReconnectScheduler scheduler;
    QSignalSpy attemptDueSpy(&scheduler, &ReconnectScheduler::attemptDue);

    scheduler.onConnectionLost(); // schedules a real 1s attempt
    QCOMPARE(scheduler.secondsRemaining(), 1);

    QVERIFY(attemptDueSpy.wait(2000)); // real QTimer/QDeadlineTimer, not simulated
    QCOMPARE(scheduler.secondsRemaining(), 0); // SPEC §20.2: "0s" need not be shown once due
}

void TestReconnectScheduler::attemptStartedHidesCountdown()
{
    ReconnectScheduler scheduler;
    scheduler.onConnectionLost();
    QVERIFY(scheduler.isScheduled());

    scheduler.onAttemptStarted();
    QVERIFY(!scheduler.isScheduled());
    QCOMPARE(scheduler.secondsRemaining(), 0);
}

// GUILESS (not APPLESS): ReconnectScheduler's QTimer needs a real
// QCoreApplication event loop to actually fire -- APPLESS creates none,
// which made QSignalSpy::wait() spin without ever seeing the timer tick.
QTEST_GUILESS_MAIN(TestReconnectScheduler)
#include "test_reconnectscheduler.moc"
