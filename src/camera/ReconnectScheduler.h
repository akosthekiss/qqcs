#pragma once

#include <QDeadlineTimer>
#include <QObject>
#include <QTimer>

// Single source of truth for reconnect state (SPEC §33). Backoff sequence
// 1/2/5/10/30s then repeating 30s. secondsRemaining() is always computed
// live from a QDeadlineTimer -- never an independently-decremented counter
// -- so both the always-on status overlay and the diagnostics overlay can
// bind to the exact same state without a separate UI timer (SPEC §20.2).
class ReconnectScheduler : public QObject
{
    Q_OBJECT

public:
    explicit ReconnectScheduler(QObject *parent = nullptr);

    // Call when a connection attempt fails (whether it was previously Live
    // or a prior reconnect attempt that itself failed). Schedules the next
    // attempt using the current backoff step, then advances the step for
    // next time (capped at the last entry, which is what makes it repeat).
    void onConnectionLost();

    // Call right when a scheduled attempt actually begins (in response to
    // attemptDue()). Hides the countdown and increments reconnectCount.
    void onAttemptStarted();

    // Call once the attempt succeeds. Resets the backoff step to 0 --
    // reconnectCount is a cumulative diagnostic and is NOT reset.
    void onConnectSucceeded();

    int secondsRemaining() const;
    int backoffSeconds() const { return m_currentBackoffSeconds; }
    int reconnectCount() const { return m_reconnectCount; }
    bool isScheduled() const { return m_scheduled; }

signals:
    void tick(); // 1 Hz while scheduled, so bound UI properties refresh
    void attemptDue();

private:
    void onTickTimeout();

    static constexpr int kBackoffStepsSeconds[] = { 1, 2, 5, 10, 30 };

    QDeadlineTimer m_deadline;
    QTimer m_tickTimer;
    int m_step = 0;
    int m_reconnectCount = 0;
    int m_currentBackoffSeconds = 0;
    bool m_scheduled = false;
};
