// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include "AppConfig.h"

#include <QString>
#include <QStringList>

// Pure, testable YAML config loading and validation (SPEC.md §6, §6.1, §7).
// Never QObject, never touches the filesystem except in loadFromFile(),
// so navigation/state-machine logic elsewhere can depend on AppConfig
// without pulling in yaml-cpp.
class ConfigLoader {
public:
    struct Result {
        bool ok = false;
        AppConfig config;
        QStringList errors;
    };

    static Result loadFromFile(const QString &path);
    static Result loadFromString(const QString &yamlText);

private:
    static Result parse(const QString &yamlText);
};
