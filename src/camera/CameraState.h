#pragma once

// SPEC.md §21, verbatim.
enum class CameraState {
    Disconnected,
    Connecting,
    Live,
    Lost
};
