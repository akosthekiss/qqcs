// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include "InputAction.h"
#include "cec/CecButton.h"

#include <QObject>

// Single funnel for discrete input sources: keyboard and CEC. Mouse wheel/
// drag are continuous and carry payload a plain enum can't hold, so
// Fullscreen.qml forwards those straight to NavigationController's
// dedicated slots instead of through here; this class only ever emits the
// same plain InputAction regardless of app state (SPEC §24:
// NavigationController alone decides what it means).
class InputManager : public QObject
{
    Q_OBJECT

public:
    explicit InputManager(QObject *parent = nullptr);

    Q_INVOKABLE void handleKeyEvent(int qtKey);
    void handleCecButtonPress(CecButton button);

signals:
    void actionTriggered(InputAction action);
};
