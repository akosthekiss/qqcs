// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

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
