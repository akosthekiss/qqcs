#pragma once

#include <QString>

// Runtime-probed (never compile-time #ifdef'd) selection of platform-specific
// GStreamer elements. Everything else in the camera subsystem is shared.
namespace PlatformGstElements {

// encodingName is the RTP SDP encoding-name, e.g. "H264" or "H265".
// Returns an empty string if no usable decoder element is installed.
QString pickVideoDecoder(const QString &encodingName);

// Whether a Qt Quick-integrated GL video sink (qml6glsink) is available.
bool qmlGlSinkAvailable();

} // namespace PlatformGstElements
