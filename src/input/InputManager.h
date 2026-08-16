#pragma once

#include "InputAction.h"

#include <QObject>

// Single funnel for discrete input sources (keyboard, CEC -- see
// CecAdapter). Mouse wheel/drag are continuous and carry payload a plain
// enum can't hold, so Fullscreen.qml forwards those straight to
// NavigationController's dedicated slots instead of through here; this
// class only ever emits the same plain InputAction regardless of app
// state (SPEC §24: NavigationController alone decides what it means).
class InputManager : public QObject
{
    Q_OBJECT

public:
    explicit InputManager(QObject *parent = nullptr);

    Q_INVOKABLE void handleKeyEvent(int qtKey);

signals:
    void actionTriggered(InputAction action);
};
