#pragma once

// SPEC.md §24, verbatim. NavigationController (not InputManager) decides
// what each action means in the current view/zoom state -- InputManager
// always emits the same action regardless of app state.
enum class InputAction {
    Up,
    Down,
    Left,
    Right,

    Select,
    Back,

    Camera0,
    Camera1,
    Camera2,
    Camera3,
    Camera4,
    Camera5,
    Camera6,
    Camera7,
    Camera8,
    Camera9,

    ZoomIn,
    ZoomOut,
    ResetZoom,

    ToggleDiagnostics
};

// Returns 0-9 for Camera0..Camera9, -1 otherwise.
inline int cameraShortcutDigit(InputAction action)
{
    switch (action) {
    case InputAction::Camera0:
        return 0;
    case InputAction::Camera1:
        return 1;
    case InputAction::Camera2:
        return 2;
    case InputAction::Camera3:
        return 3;
    case InputAction::Camera4:
        return 4;
    case InputAction::Camera5:
        return 5;
    case InputAction::Camera6:
        return 6;
    case InputAction::Camera7:
        return 7;
    case InputAction::Camera8:
        return 8;
    case InputAction::Camera9:
        return 9;
    default:
        return -1;
    }
}
