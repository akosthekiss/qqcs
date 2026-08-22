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
    // SPEC §6.2: "cover" | "contain" | "fill". Kept as plain strings here
    // (not AppSinkVideoItem::FillMode) so the config layer stays free of
    // the camera subsystem's types; CameraManager maps them where consumed.
    QString mosaicFillMode = QStringLiteral("cover");
    QString fullscreenFillMode = QStringLiteral("contain");
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
