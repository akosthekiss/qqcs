// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

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
