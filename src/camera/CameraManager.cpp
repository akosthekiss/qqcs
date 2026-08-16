#include "CameraManager.h"

#include "StreamUrlPolicy.h"

CameraManager::CameraManager(AppConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_listModel(new CameraListModel(m_config.cameras, this))
{
    m_runtimes.reserve(m_config.cameras.size());
    for (int row = 0; row < m_config.cameras.size(); ++row) {
        const CameraConfig &camera = m_config.cameras.at(row);

        CameraRuntime runtime;
        runtime.config = camera;
        runtime.mosaicPipeline = new RtspStreamPipeline(StreamUrlPolicy::mosaicUrl(camera), this);

        connect(runtime.mosaicPipeline, &RtspStreamPipeline::stateChanged, m_listModel,
                [this, row](CameraState state) { m_listModel->setState(row, state); });

        m_runtimes.push_back(std::move(runtime));
    }
}

void CameraManager::start()
{
    if (m_started)
        return;
    m_started = true;
    for (auto &runtime : m_runtimes)
        runtime.mosaicPipeline->start();
}

CameraManager::CameraRuntime *CameraManager::runtimeForId(const QString &id)
{
    for (auto &runtime : m_runtimes) {
        if (runtime.config.id == id)
            return &runtime;
    }
    return nullptr;
}

AppSinkVideoItem *CameraManager::mosaicVideoItem(const QString &id) const
{
    for (const auto &runtime : m_runtimes) {
        if (runtime.config.id == id)
            return runtime.mosaicPipeline->videoItem();
    }
    return nullptr;
}

AppSinkVideoItem *CameraManager::fullscreenVideoItem() const
{
    return m_fullscreenPipeline ? m_fullscreenPipeline->videoItem() : nullptr;
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
    teardownFullscreenPipeline();
    m_fullscreenId.clear();
    emit fullscreenIdChanged(m_fullscreenId);
}

void CameraManager::switchFullscreenCamera(const QString &id)
{
    if (m_fullscreenId == id)
        return;

    CameraRuntime *runtime = runtimeForId(id);
    if (!runtime)
        return;

    teardownFullscreenPipeline();

    m_fullscreenPipeline = std::make_unique<RtspStreamPipeline>(StreamUrlPolicy::fullscreenUrl(runtime->config));
    if (m_started)
        m_fullscreenPipeline->start();

    m_fullscreenId = id;
    emit fullscreenIdChanged(m_fullscreenId);
}

void CameraManager::teardownFullscreenPipeline()
{
    if (!m_fullscreenPipeline)
        return;
    m_fullscreenPipeline->stop();
    m_fullscreenPipeline.reset();
}
