#include "InputManager.h"

#include "KeyboardMapping.h"

InputManager::InputManager(QObject *parent) : QObject(parent) { }

void InputManager::handleKeyEvent(int qtKey)
{
    InputAction action;
    if (KeyboardMapping::mapKey(qtKey, action))
        emit actionTriggered(action);
    // Unmapped keys are a no-op, not an error -- most keys have no meaning here.
}
