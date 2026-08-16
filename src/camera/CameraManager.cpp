#include "CameraManager.h"

CameraManager::CameraManager(AppConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_listModel(new CameraListModel(m_config.cameras, this))
{
}

void CameraManager::focus(const QString &id)
{
    Q_UNUSED(id);
    // No pipeline effect (SPEC §5): focusing a mosaic tile does not start
    // or stop anything, it is purely a NavigationController concern.
}

void CameraManager::enterFullscreen(const QString &id)
{
    switchFullscreenCamera(id);
}

void CameraManager::exitFullscreen()
{
    if (m_fullscreenId.isEmpty())
        return;
    m_fullscreenId.clear();
    emit fullscreenIdChanged(m_fullscreenId);
}

void CameraManager::switchFullscreenCamera(const QString &id)
{
    if (m_fullscreenId == id)
        return;
    m_fullscreenId = id;
    emit fullscreenIdChanged(m_fullscreenId);
}
