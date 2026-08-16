#pragma once

#include "AppConfig.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// QML-facing wrapper around ConfigLoader. Owns the one AppConfig instance
// the rest of the app (CameraManager, NavigationController) reads from.
class ConfigModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
    Q_PROPERTY(QString configPath READ configPath NOTIFY configPathChanged)
    Q_PROPERTY(QStringList errors READ errors NOTIFY validChanged)
    Q_PROPERTY(int columns READ columns NOTIFY validChanged)
    Q_PROPERTY(QVariantMap overlay READ overlay NOTIFY validChanged)

public:
    explicit ConfigModel(QObject *parent = nullptr);

    bool load(const QString &path);
    const AppConfig &appConfig() const { return m_config; }

    bool isValid() const { return m_valid; }
    QString configPath() const { return m_configPath; }
    QStringList errors() const { return m_errors; }
    int columns() const { return m_config.layout.columns; }
    QVariantMap overlay() const
    {
        return {
            { QStringLiteral("enabled"), m_config.overlay.enabled },
            { QStringLiteral("position"), m_config.overlay.position },
            { QStringLiteral("showName"), m_config.overlay.showName },
            { QStringLiteral("showStatus"), m_config.overlay.showStatus },
        };
    }

    Q_INVOKABLE bool reload();

    // First existing path among /etc/qqcs/config.yaml, ~/.config/qqcs/config.yaml,
    // ./config.yaml (in that order); falls back to ./config.yaml if none exist.
    // main.cpp checks --config and $QQCS_CONFIG before falling back to this.
    static QString resolveDefaultPath();

signals:
    void validChanged();
    void configPathChanged();

private:
    AppConfig m_config;
    QString m_configPath;
    QStringList m_errors;
    bool m_valid = false;
};
