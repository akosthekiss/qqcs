#pragma once

#include <QString>

// SPEC §22's field list, minus latency: true glass-to-glass latency needs
// RTCP NTP correlation that's unreliable across consumer cameras, and the
// spec explicitly allows omitting a field that can't be measured
// reliably -- rather than carry a field that could only ever be N/A, it's
// left out here and noted in the README instead.
struct Diagnostics {
    QString videoCodec;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    qint64 bitrateBps = 0;
    QString audioCodec;
    QString rtspTransport;
    qint64 droppedFrames = 0;
    int reconnectCount = 0;
    int reconnectBackoffSeconds = 0;
    int reconnectCountdownSeconds = 0;
    QString rtspUrl;
};
