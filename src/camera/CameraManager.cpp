#include "CameraManager.h"

#include "AppSinkVideoItem.h"
#include "StreamUrlPolicy.h"

#include <QQmlProperty>
#include <QVariant>

namespace {

void attachVideoItem(AppSinkVideoItem *videoItem, QQuickItem *container)
{
    if (!videoItem || !container)
        return;
    videoItem->setParentItem(container);
    QQmlProperty(videoItem, QStringLiteral("anchors.fill")).write(QVariant::fromValue(container));
}

} // namespace

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

void CameraManager::attachMosaicVideo(const QString &id, QQuickItem *container)
{
    AppSinkVideoItem *item = mosaicVideoItem(id);
    if (item)
        item->setFillMode(AppSinkVideoItem::FillMode::Cover); // SPEC §9: mosaic is COVER
    attachVideoItem(item, container);
}

void CameraManager::attachFullscreenVideo(QQuickItem *container)
{
    AppSinkVideoItem *item = fullscreenVideoItem();
    if (item)
        item->setFillMode(AppSinkVideoItem::FillMode::Contain); // SPEC §11: fullscreen 1.0x is CONTAIN
    attachVideoItem(item, container);
}

QStringList CameraManager::cameraIds() const
{
    QStringList ids;
    ids.reserve(m_runtimes.size());
    for (const auto &runtime : m_runtimes)
        ids << runtime.config.id;
    return ids;
}

QHash<int, int> CameraManager::shortcutDigitToIndex() const
{
    QHash<int, int> map;
    for (int i = 0; i < m_runtimes.size(); ++i) {
        const CameraConfig &config = m_runtimes.at(i).config;
        if (config.hasShortcut())
            map[config.shortcut] = i;
    }
    return map;
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
