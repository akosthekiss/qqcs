// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "PlatformGstElements.h"

#include <gst/gst.h>

namespace {

bool elementAvailable(const char *name)
{
    GstElementFactory *factory = gst_element_factory_find(name);
    if (!factory)
        return false;
    gst_object_unref(factory);
    return true;
}

} // namespace

namespace PlatformGstElements {

QString pickVideoDecoder(const QString &encodingName)
{
    // vtdec (macOS VideoToolbox) is a single generic element that negotiates
    // its codec from input caps, so it covers both H.264 and H.265.
    if (elementAvailable("vtdec"))
        return QStringLiteral("vtdec");

    const bool isH265 = encodingName.compare(QStringLiteral("H265"), Qt::CaseInsensitive) == 0;
    if (isH265) {
        if (elementAvailable("vah265dec"))
            return QStringLiteral("vah265dec");
        if (elementAvailable("avdec_h265"))
            return QStringLiteral("avdec_h265");
    } else {
        if (elementAvailable("vah264dec"))
            return QStringLiteral("vah264dec");
        if (elementAvailable("avdec_h264"))
            return QStringLiteral("avdec_h264");
    }
    return {};
}

bool qmlGlSinkAvailable()
{
    return elementAvailable("qml6glsink");
}

} // namespace PlatformGstElements
