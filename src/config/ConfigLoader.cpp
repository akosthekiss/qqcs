// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#include "ConfigLoader.h"

#include <yaml-cpp/yaml.h>

#include <QFile>
#include <QLoggingCategory>
#include <QSet>
#include <QTextStream>

namespace {

Q_LOGGING_CATEGORY(lcConfig, "qqcs.config")

template <typename T>
bool tryAs(const YAML::Node &node, T &out)
{
    if (!node || !node.IsScalar())
        return false;
    try {
        out = node.as<T>();
        return true;
    } catch (const YAML::Exception &) {
        return false;
    }
}

QString asQString(const YAML::Node &node)
{
    std::string s;
    return tryAs(node, s) ? QString::fromStdString(s) : QString();
}

const QSet<QString> &validOverlayPositions()
{
    static const QSet<QString> positions{QStringLiteral("top"), QStringLiteral("bottom")};
    return positions;
}

} // namespace

ConfigLoader::Result ConfigLoader::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Result result;
        result.errors << QStringLiteral("Cannot open config file: %1").arg(path);
        qCritical(lcConfig).noquote() << result.errors.last();
        return result;
    }
    QTextStream stream(&file);
    return parse(stream.readAll());
}

ConfigLoader::Result ConfigLoader::loadFromString(const QString &yamlText)
{
    return parse(yamlText);
}

ConfigLoader::Result ConfigLoader::parse(const QString &yamlText)
{
    Result result;

    YAML::Node root;
    try {
        root = YAML::Load(yamlText.toStdString());
    } catch (const YAML::Exception &e) {
        result.errors << QStringLiteral("YAML syntax error at line %1: %2")
                             .arg(e.mark.line + 1)
                             .arg(QString::fromStdString(e.msg));
        for (const auto &err : result.errors)
            qCritical(lcConfig).noquote() << err;
        return result;
    }

    if (!root || !root.IsMap()) {
        result.errors << QStringLiteral("Top-level YAML document must be a map");
        qCritical(lcConfig).noquote() << result.errors.last();
        return result;
    }

    if (const auto layoutNode = root["layout"]) {
        if (!layoutNode.IsMap()) {
            result.errors << QStringLiteral("layout: must be a map");
        } else if (const auto colNode = layoutNode["columns"]) {
            int columns = 0;
            if (!tryAs(colNode, columns) || columns <= 0)
                result.errors << QStringLiteral("layout.columns: must be a positive integer");
            else
                result.config.layout.columns = columns;
        }
    }

    if (const auto overlayNode = root["overlay"]) {
        if (!overlayNode.IsMap()) {
            result.errors << QStringLiteral("overlay: must be a map");
        } else {
            bool b = false;
            if (const auto n = overlayNode["enabled"]; n && tryAs(n, b))
                result.config.overlay.enabled = b;
            if (const auto n = overlayNode["showName"]; n && tryAs(n, b))
                result.config.overlay.showName = b;
            if (const auto n = overlayNode["showStatus"]; n && tryAs(n, b))
                result.config.overlay.showStatus = b;
            if (const auto n = overlayNode["position"]) {
                const QString pos = asQString(n);
                if (!validOverlayPositions().contains(pos))
                    result.errors << QStringLiteral(
                                         "overlay.position: invalid value '%1' (expected one of: top, bottom)")
                                         .arg(pos);
                else
                    result.config.overlay.position = pos;
            }
        }
    }

    const auto camerasNode = root["cameras"];
    if (!camerasNode || !camerasNode.IsSequence() || camerasNode.size() == 0) {
        result.errors << QStringLiteral("cameras: must be a non-empty list");
    } else {
        QSet<QString> seenIds;
        QSet<int> seenShortcuts;
        for (std::size_t i = 0; i < camerasNode.size(); ++i) {
            const auto camNode = camerasNode[i];
            const QString label = QStringLiteral("cameras[%1]").arg(i);
            if (!camNode.IsMap()) {
                result.errors << QStringLiteral("%1: must be a map").arg(label);
                continue;
            }

            CameraConfig cam;
            cam.id = asQString(camNode["id"]);
            if (cam.id.isEmpty()) {
                result.errors << QStringLiteral("%1: 'id' is required").arg(label);
            } else if (seenIds.contains(cam.id)) {
                result.errors << QStringLiteral("%1: duplicate camera id '%2'").arg(label, cam.id);
            } else {
                seenIds.insert(cam.id);
            }

            cam.name = asQString(camNode["name"]);
            cam.mainUrl = asQString(camNode["mainUrl"]);
            if (cam.mainUrl.isEmpty())
                result.errors << QStringLiteral("%1 (id=%2): 'mainUrl' is required").arg(label, cam.id);
            cam.subUrl = asQString(camNode["subUrl"]);

            if (const auto shortcutNode = camNode["shortcut"]) {
                int shortcut = -1;
                if (!tryAs(shortcutNode, shortcut) || shortcut < 0 || shortcut > 9) {
                    result.errors << QStringLiteral("%1 (id=%2): 'shortcut' must be an integer 0-9")
                                         .arg(label, cam.id);
                } else if (seenShortcuts.contains(shortcut)) {
                    result.errors << QStringLiteral("%1 (id=%2): duplicate shortcut %3")
                                         .arg(label, cam.id)
                                         .arg(shortcut);
                } else {
                    seenShortcuts.insert(shortcut);
                    cam.shortcut = shortcut;
                }
            }

            result.config.cameras.push_back(cam);
        }
    }

    result.ok = result.errors.isEmpty();
    if (!result.ok) {
        for (const auto &err : result.errors)
            qCritical(lcConfig).noquote() << err;
    }
    return result;
}
