// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include "InputAction.h"

#include <Qt>

// Pure, testable keyboard->InputAction mapping (SPEC §26). Deliberately no
// mapping to Back: keyboard's only path to Back is Escape->ResetZoom,
// whose "or Back" behavior at zoom==1.0 is state-dependent logic already
// inside NavigationController::handleInputAction, never a second mapping
// here -- this is what keeps 0 (Camera0) and Escape (ResetZoom)
// structurally unable to collide.
namespace KeyboardMapping {

inline bool mapKey(int qtKey, InputAction &outAction)
{
    switch (qtKey) {
    case Qt::Key_Up:
        outAction = InputAction::Up;
        return true;
    case Qt::Key_Down:
        outAction = InputAction::Down;
        return true;
    case Qt::Key_Left:
        outAction = InputAction::Left;
        return true;
    case Qt::Key_Right:
        outAction = InputAction::Right;
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        outAction = InputAction::Select;
        return true;
    case Qt::Key_Escape:
        outAction = InputAction::ResetZoom;
        return true;
    case Qt::Key_0:
        outAction = InputAction::Camera0;
        return true;
    case Qt::Key_1:
        outAction = InputAction::Camera1;
        return true;
    case Qt::Key_2:
        outAction = InputAction::Camera2;
        return true;
    case Qt::Key_3:
        outAction = InputAction::Camera3;
        return true;
    case Qt::Key_4:
        outAction = InputAction::Camera4;
        return true;
    case Qt::Key_5:
        outAction = InputAction::Camera5;
        return true;
    case Qt::Key_6:
        outAction = InputAction::Camera6;
        return true;
    case Qt::Key_7:
        outAction = InputAction::Camera7;
        return true;
    case Qt::Key_8:
        outAction = InputAction::Camera8;
        return true;
    case Qt::Key_9:
        outAction = InputAction::Camera9;
        return true;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        outAction = InputAction::ZoomIn;
        return true;
    case Qt::Key_Minus:
        outAction = InputAction::ZoomOut;
        return true;
    case Qt::Key_I:
        outAction = InputAction::ToggleDiagnostics;
        return true;
    default:
        return false;
    }
}

} // namespace KeyboardMapping
