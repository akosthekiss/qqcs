#include "CameraListModel.h"

CameraListModel::CameraListModel(QVector<CameraConfig> cameras, QObject *parent)
    : QAbstractListModel(parent)
{
    m_entries.reserve(cameras.size());
    for (auto &config : cameras) {
        Entry entry;
        entry.config = std::move(config);
        m_entries.push_back(std::move(entry));
    }
}

int CameraListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant CameraListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case IdRole:
        return entry.config.id;
    case NameRole:
        return entry.config.name;
    case ShortcutRole:
        return entry.config.shortcut;
    case StateRole:
        return static_cast<int>(entry.state);
    case HasAudioRole:
        return entry.hasAudio;
    case MainUrlRole:
        return entry.config.mainUrl;
    case SubUrlRole:
        return entry.config.subUrl;
    case ReconnectSecondsRole:
        return entry.reconnectSeconds;
    case ReconnectBackoffRole:
        return entry.reconnectBackoff;
    case ReconnectCountRole:
        return entry.reconnectCount;
    default:
        return {};
    }
}

QHash<int, QByteArray> CameraListModel::roleNames() const
{
    return {
        { IdRole, "id" },
        { NameRole, "name" },
        { ShortcutRole, "shortcut" },
        { StateRole, "state" },
        { HasAudioRole, "hasAudio" },
        { MainUrlRole, "mainUrl" },
        { SubUrlRole, "subUrl" },
        { ReconnectSecondsRole, "reconnectSeconds" },
        { ReconnectBackoffRole, "reconnectBackoff" },
        { ReconnectCountRole, "reconnectCount" },
    };
}

int CameraListModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).config.id == id)
            return i;
    }
    return -1;
}

void CameraListModel::setState(int row, CameraState state)
{
    if (row < 0 || row >= m_entries.size())
        return;
    m_entries[row].state = state;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { StateRole });
}

void CameraListModel::setHasAudio(int row, bool hasAudio)
{
    if (row < 0 || row >= m_entries.size())
        return;
    m_entries[row].hasAudio = hasAudio;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { HasAudioRole });
}

void CameraListModel::setReconnectInfo(int row, int secondsRemaining, int backoffSeconds, int reconnectCount)
{
    if (row < 0 || row >= m_entries.size())
        return;
    auto &entry = m_entries[row];
    entry.reconnectSeconds = secondsRemaining;
    entry.reconnectBackoff = backoffSeconds;
    entry.reconnectCount = reconnectCount;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { ReconnectSecondsRole, ReconnectBackoffRole, ReconnectCountRole });
}
