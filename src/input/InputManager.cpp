// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "InputManager.h"

#include "KeyboardMapping.h"
#include "cec/CecMapping.h"

InputManager::InputManager(QObject *parent) : QObject(parent) { }

void InputManager::handleKeyEvent(int qtKey)
{
    InputAction action;
    if (KeyboardMapping::mapKey(qtKey, action))
        emit actionTriggered(action);
    // Unmapped keys are a no-op, not an error -- most keys have no meaning here.
}

void InputManager::handleCecButtonPress(CecButton button)
{
    InputAction action;
    if (CecMapping::mapButton(button, action))
        emit actionTriggered(action);
    // Yellow (reserved, SPEC §36) and anything unmapped are a no-op.
}
