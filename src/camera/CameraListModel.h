#pragma once

#include "CameraState.h"
#include "config/AppConfig.h"

#include <QAbstractListModel>
#include <QVector>

// Read-only data seam handed to the UI layer (SPEC §5: "GUI ne kezelje
// közvetlenül... a GStreamer pipeline-okat"). Camera order == YAML order,
// which is also the canonical navigation order used by NavMath.
class CameraListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    // Role string is "cameraId", not "id" -- QML's `id:` is a reserved
    // keyword and clashes with a delegate `required property string id`.
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ShortcutRole,
        StateRole,
        HasAudioRole,
        MainUrlRole,
        SubUrlRole,
        ReconnectSecondsRole,
        ReconnectBackoffRole,
        ReconnectCountRole,
    };

    struct Entry {
        CameraConfig config;
        CameraState state = CameraState::Disconnected;
        bool hasAudio = false;
        int reconnectSeconds = 0;
        int reconnectBackoff = 0;
        int reconnectCount = 0;
    };

    explicit CameraListModel(QVector<CameraConfig> cameras, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int indexOfId(const QString &id) const;
    int count() const { return m_entries.size(); }
    const Entry &entryAt(int row) const { return m_entries.at(row); }

    void setState(int row, CameraState state);
    void setHasAudio(int row, bool hasAudio);
    void setReconnectInfo(int row, int secondsRemaining, int backoffSeconds, int reconnectCount);

private:
    QVector<Entry> m_entries;
};
