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
