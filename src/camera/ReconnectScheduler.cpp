#include "ReconnectScheduler.h"

#include <algorithm>
#include <cmath>
#include <iterator>

ReconnectScheduler::ReconnectScheduler(QObject *parent) : QObject(parent)
{
    m_tickTimer.setInterval(1000);
    connect(&m_tickTimer, &QTimer::timeout, this, &ReconnectScheduler::onTickTimeout);
}

void ReconnectScheduler::onConnectionLost()
{
    const int stepIndex = std::min<int>(m_step, std::size(kBackoffStepsSeconds) - 1);
    m_currentBackoffSeconds = kBackoffStepsSeconds[stepIndex];
    m_deadline = QDeadlineTimer(m_currentBackoffSeconds * 1000);
    m_scheduled = true;
    if (m_step < static_cast<int>(std::size(kBackoffStepsSeconds)) - 1)
        ++m_step;

    if (!m_tickTimer.isActive())
        m_tickTimer.start();
    emit tick();
}

void ReconnectScheduler::onAttemptStarted()
{
    m_scheduled = false;
    ++m_reconnectCount;
    emit tick();
}

void ReconnectScheduler::onConnectSucceeded()
{
    m_step = 0;
    m_scheduled = false;
    m_tickTimer.stop();
    emit tick();
}

int ReconnectScheduler::secondsRemaining() const
{
    if (!m_scheduled)
        return 0;
    return std::max<qint64>(0, static_cast<qint64>(std::ceil(m_deadline.remainingTime() / 1000.0)));
}

void ReconnectScheduler::onTickTimeout()
{
    emit tick();
    if (m_scheduled && m_deadline.hasExpired())
        emit attemptDue();
}
