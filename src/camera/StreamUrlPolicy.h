#pragma once

#include "config/AppConfig.h"

// SPEC.md §32: mosaic prefers the substream when present, falls back to
// mainUrl otherwise. Fullscreen (and its audio) always uses mainUrl.
namespace StreamUrlPolicy {

inline QString mosaicUrl(const CameraConfig &camera)
{
    return camera.hasSub() ? camera.subUrl : camera.mainUrl;
}

inline QString fullscreenUrl(const CameraConfig &camera)
{
    return camera.mainUrl;
}

} // namespace StreamUrlPolicy
