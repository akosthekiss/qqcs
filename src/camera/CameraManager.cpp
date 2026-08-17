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
        connect(runtime.mosaicPipeline, &RtspStreamPipeline::reconnectInfoChanged, m_listModel, [this, row] {
            RtspStreamPipeline *pipeline = m_runtimes.at(row).mosaicPipeline;
            m_listModel->setReconnectInfo(row, pipeline->reconnectSecondsRemaining(), pipeline->reconnectBackoffSeconds(),
                                           pipeline->reconnectCount());
        });
        connect(runtime.mosaicPipeline, &RtspStreamPipeline::hasAudioChanged, m_listModel,
                [this, row](bool hasAudio) { m_listModel->setHasAudio(row, hasAudio); });

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

    // SPEC §13: the previous camera's audio must stop before the new one's
    // starts. teardownFullscreenPipeline() stops the whole old pipeline
    // (video included) right after, but disabling audio explicitly first
    // keeps the ordering intent visible and correct even if that changes.
    if (m_fullscreenPipeline)
        m_fullscreenPipeline->enableAudio(false);
    teardownFullscreenPipeline();

    m_fullscreenPipeline = std::make_unique<RtspStreamPipeline>(StreamUrlPolicy::fullscreenUrl(runtime->config));
    connect(m_fullscreenPipeline.get(), &RtspStreamPipeline::stateChanged, this, &CameraManager::fullscreenStatusChanged);
    connect(m_fullscreenPipeline.get(), &RtspStreamPipeline::reconnectInfoChanged, this,
            &CameraManager::fullscreenStatusChanged);
    connect(m_fullscreenPipeline.get(), &RtspStreamPipeline::hasAudioChanged, this,
            &CameraManager::fullscreenStatusChanged);
    if (m_started)
        m_fullscreenPipeline->start();
    m_fullscreenPipeline->enableAudio(true); // SPEC §13: fullscreen audio is automatic, no config flag

    m_fullscreenId = id;
    emit fullscreenIdChanged(m_fullscreenId);
    emit fullscreenStatusChanged();
}

void CameraManager::teardownFullscreenPipeline()
{
    if (!m_fullscreenPipeline)
        return;
    m_fullscreenPipeline->enableAudio(false);
    m_fullscreenPipeline->stop();
    m_fullscreenPipeline.reset();
    emit fullscreenStatusChanged();
}

QVariantMap CameraManager::fullscreenStatus() const
{
    if (!m_fullscreenPipeline)
        return {};
    const CameraRuntime *runtime = nullptr;
    for (const auto &r : m_runtimes) {
        if (r.config.id == m_fullscreenId) {
            runtime = &r;
            break;
        }
    }
    return {
        { QStringLiteral("cameraId"), m_fullscreenId },
        { QStringLiteral("name"), runtime ? runtime->config.name : QString() },
        { QStringLiteral("state"), static_cast<int>(m_fullscreenPipeline->state()) },
        { QStringLiteral("hasAudio"), m_fullscreenPipeline->hasAudio() },
        { QStringLiteral("reconnectSeconds"), m_fullscreenPipeline->reconnectSecondsRemaining() },
        { QStringLiteral("reconnectBackoff"), m_fullscreenPipeline->reconnectBackoffSeconds() },
        { QStringLiteral("reconnectCount"), m_fullscreenPipeline->reconnectCount() },
    };
}

QVariantMap CameraManager::fullscreenDiagnostics() const
{
    if (!m_fullscreenPipeline)
        return {};
    const Diagnostics d = m_fullscreenPipeline->diagnostics();
    return {
        { QStringLiteral("videoCodec"), d.videoCodec },
        { QStringLiteral("width"), d.width },
        { QStringLiteral("height"), d.height },
        { QStringLiteral("fps"), d.fps },
        { QStringLiteral("bitrateBps"), d.bitrateBps },
        { QStringLiteral("audioCodec"), d.audioCodec },
        { QStringLiteral("rtspTransport"), d.rtspTransport },
        { QStringLiteral("droppedFrames"), d.droppedFrames },
        { QStringLiteral("reconnectCount"), d.reconnectCount },
        { QStringLiteral("reconnectBackoffSeconds"), d.reconnectBackoffSeconds },
        { QStringLiteral("reconnectCountdownSeconds"), d.reconnectCountdownSeconds },
        { QStringLiteral("rtspUrl"), d.rtspUrl },
    };
}
