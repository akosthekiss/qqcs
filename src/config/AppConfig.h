// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

#pragma once

#include <QString>
#include <QVector>

struct CameraConfig {
    QString id;
    QString name;
    int shortcut = -1;
    QString mainUrl;
    QString subUrl;

    bool hasName() const { return !name.isEmpty(); }
    bool hasSub() const { return !subUrl.isEmpty(); }
    bool hasShortcut() const { return shortcut >= 0; }
};

struct LayoutConfig {
    int columns = 4;
};

struct OverlayConfig {
    bool enabled = true;
    QString position = QStringLiteral("bottom");
    bool showName = true;
    bool showStatus = true;
};

struct AppConfig {
    LayoutConfig layout;
    OverlayConfig overlay;
    QVector<CameraConfig> cameras;
};
