#pragma once

// SPEC §28: buttons libCEC must support. Yellow is reserved (SPEC §36) --
// it exists here so the architecture can assign it a meaning later, but
// CecMapping::mapButton() deliberately never maps it to an InputAction.
enum class CecButton {
    Up,
    Down,
    Left,
    Right,
    Select,
    Back,
    Red,
    Green,
    Blue,
    Yellow,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
};
