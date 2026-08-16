#include "ConfigModel.h"
#include "ConfigLoader.h"

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
