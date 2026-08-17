// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

// SPEC.md §21, verbatim.
enum class CameraState {
    Disconnected,
    Connecting,
    Live,
    Lost
};
