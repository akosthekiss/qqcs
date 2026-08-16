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
