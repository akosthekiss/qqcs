// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "ConfigModel.h"
#include "ConfigLoader.h"

#include <QDir>
#include <QFile>

ConfigModel::ConfigModel(QObject *parent) : QObject(parent) { }

bool ConfigModel::load(const QString &path)
{
    m_configPath = path;
    emit configPathChanged();
    return reload();
}

bool ConfigModel::reload()
{
    const auto result = ConfigLoader::loadFromFile(m_configPath);
    m_valid = result.ok;
    m_config = result.config;
    m_errors = result.errors;
    emit validChanged();
    return m_valid;
}

QString ConfigModel::resolveDefaultPath()
{
    const QStringList candidates = {
        QStringLiteral("/etc/qqcs/config.yaml"),
        QDir::homePath() + QStringLiteral("/.config/qqcs/config.yaml"),
        QStringLiteral("./config.yaml"),
    };
    for (const auto &candidate : candidates) {
        if (QFile::exists(candidate))
            return candidate;
    }
    return candidates.last();
}
