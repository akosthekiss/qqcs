#pragma once

#include <QString>

// SPEC §22's field list. latencyMs stays -1 (N/A) deliberately -- true
// glass-to-glass latency needs RTCP NTP correlation that's unreliable
// across consumer cameras, and the spec explicitly allows N/A when a
// field "nem megbízhatóan mérhető".
struct Diagnostics {
    QString videoCodec;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    qint64 bitrateBps = 0;
    QString audioCodec;
    QString rtspTransport;
    int latencyMs = -1;
    qint64 droppedFrames = 0;
    int reconnectCount = 0;
    int reconnectBackoffSeconds = 0;
    int reconnectCountdownSeconds = 0;
    QString rtspUrl;
};
