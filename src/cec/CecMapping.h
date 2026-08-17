// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include "CecButton.h"
#include "input/InputAction.h"

// Pure, testable CecButton->InputAction mapping (SPEC §25). Yellow is
// reserved (SPEC §36) and deliberately never maps to anything.
namespace CecMapping {

inline bool mapButton(CecButton button, InputAction &outAction)
{
    switch (button) {
    case CecButton::Up:
        outAction = InputAction::Up;
        return true;
    case CecButton::Down:
        outAction = InputAction::Down;
        return true;
    case CecButton::Left:
        outAction = InputAction::Left;
        return true;
    case CecButton::Right:
        outAction = InputAction::Right;
        return true;
    case CecButton::Select:
        outAction = InputAction::Select;
        return true;
    case CecButton::Back:
        outAction = InputAction::Back;
        return true;
    case CecButton::Red:
        outAction = InputAction::ZoomIn;
        return true;
    case CecButton::Green:
        outAction = InputAction::ZoomOut;
        return true;
    case CecButton::Blue:
        outAction = InputAction::ToggleDiagnostics;
        return true;
    case CecButton::Yellow:
        return false; // reserved, SPEC §36
    case CecButton::Digit0:
        outAction = InputAction::Camera0;
        return true;
    case CecButton::Digit1:
        outAction = InputAction::Camera1;
        return true;
    case CecButton::Digit2:
        outAction = InputAction::Camera2;
        return true;
    case CecButton::Digit3:
        outAction = InputAction::Camera3;
        return true;
    case CecButton::Digit4:
        outAction = InputAction::Camera4;
        return true;
    case CecButton::Digit5:
        outAction = InputAction::Camera5;
        return true;
    case CecButton::Digit6:
        outAction = InputAction::Camera6;
        return true;
    case CecButton::Digit7:
        outAction = InputAction::Camera7;
        return true;
    case CecButton::Digit8:
        outAction = InputAction::Camera8;
        return true;
    case CecButton::Digit9:
        outAction = InputAction::Camera9;
        return true;
    }
    return false;
}

} // namespace CecMapping
