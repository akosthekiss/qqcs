// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

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

// ConfigLoader already validated this to one of "cover"/"contain"/"fill"
// (SPEC §6.2); the fallback here is just defensive, not a second
// validation pass.
AppSinkVideoItem::FillMode toFillMode(const QString &mode, AppSinkVideoItem::FillMode fallback)
{
    if (mode == QStringLiteral("cover"))
        return AppSinkVideoItem::FillMode::Cover;
    if (mode == QStringLiteral("contain"))
        return AppSinkVideoItem::FillMode::Contain;
    if (mode == QStringLiteral("fill"))
        return AppSinkVideoItem::FillMode::Fill;
    return fallback;
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
        runtime.config = &camera;
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
        if (runtime.config->id == id)
            return &runtime;
    }
    return nullptr;
}

AppSinkVideoItem *CameraManager::mosaicVideoItem(const QString &id) const
{
    for (const auto &runtime : m_runtimes) {
        if (runtime.config->id == id)
            return runtime.mosaicPipeline->videoItem();
    }
    return nullptr;
}

AppSinkVideoItem *CameraManager::fullscreenVideoItem() const
{
    return m_fullscreen ? m_fullscreen->pipeline->videoItem() : nullptr;
}

QSizeF CameraManager::fullscreenContentSize() const
{
    return m_fullscreen ? m_fullscreen->pipeline->videoItem()->contentSize() : QSizeF();
}

void CameraManager::setFullscreenAvailableSize(QSizeF size)
{
    m_fullscreenAvailableSize = size;
    if (m_fullscreen)
        m_fullscreen->pipeline->videoItem()->setAvailableSize(size);
}

void CameraManager::attachMosaicVideo(const QString &id, QQuickItem *container)
{
    AppSinkVideoItem *item = mosaicVideoItem(id);
    if (item) // SPEC §6.2/§9: mosaic defaults to COVER, configurable
        item->setFillMode(toFillMode(m_config.layout.mosaicFillMode, AppSinkVideoItem::FillMode::Cover));
    attachVideoItem(item, container);
}

void CameraManager::attachFullscreenVideo(QQuickItem *container)
{
    AppSinkVideoItem *item = fullscreenVideoItem();
    if (item) // SPEC §6.2/§11: fullscreen 1.0x defaults to CONTAIN, configurable
        item->setFillMode(toFillMode(m_config.layout.fullscreenFillMode, AppSinkVideoItem::FillMode::Contain));
    attachVideoItem(item, container);
}

QStringList CameraManager::cameraIds() const
{
    QStringList ids;
    ids.reserve(m_runtimes.size());
    for (const auto &runtime : m_runtimes)
        ids << runtime.config->id;
    return ids;
}

QHash<int, int> CameraManager::shortcutDigitToIndex() const
{
    QHash<int, int> map;
    for (int i = 0; i < m_runtimes.size(); ++i) {
        const CameraConfig &config = *m_runtimes.at(i).config;
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
    if (!m_fullscreen)
        return;
    teardownFullscreenPipeline();
    emit fullscreenIdChanged(QString());
}

void CameraManager::switchFullscreenCamera(const QString &id)
{
    if (m_fullscreen && m_fullscreen->cameraId == id)
        return;

    CameraRuntime *runtime = runtimeForId(id);
    if (!runtime)
        return;

    // SPEC §13: the previous camera's audio must stop before the new one's
    // starts. teardownFullscreenPipeline() stops the whole old pipeline
    // (video included) right after, but disabling audio explicitly first
    // keeps the ordering intent visible and correct even if that changes.
    if (m_fullscreen)
        m_fullscreen->pipeline->enableAudio(false);
    teardownFullscreenPipeline();

    auto pipeline = std::make_unique<RtspStreamPipeline>(StreamUrlPolicy::fullscreenUrl(*runtime->config));
    connect(pipeline.get(), &RtspStreamPipeline::stateChanged, this, &CameraManager::fullscreenStatusChanged);
    connect(pipeline.get(), &RtspStreamPipeline::reconnectInfoChanged, this, &CameraManager::fullscreenStatusChanged);
    connect(pipeline.get(), &RtspStreamPipeline::hasAudioChanged, this, &CameraManager::fullscreenStatusChanged);
    // SPEC §34: re-forwarded on every switch since it's a new AppSinkVideoItem
    // each time -- see fullscreenContentSize()'s own comment.
    connect(pipeline->videoItem(), &AppSinkVideoItem::contentSizeChanged, this,
            &CameraManager::fullscreenContentSizeChanged);
    pipeline->videoItem()->setAvailableSize(m_fullscreenAvailableSize);
    if (m_started)
        pipeline->start();
    pipeline->enableAudio(true); // SPEC §13: fullscreen audio is automatic, no config flag

    m_fullscreen = FullscreenSession{ id, std::move(pipeline) };
    emit fullscreenIdChanged(id);
    emit fullscreenStatusChanged();
    emit fullscreenContentSizeChanged();
}

void CameraManager::teardownFullscreenPipeline()
{
    if (!m_fullscreen)
        return;
    m_fullscreen->pipeline->enableAudio(false);
    m_fullscreen->pipeline->stop();
    m_fullscreen.reset();
    emit fullscreenStatusChanged();
}

QVariantMap CameraManager::fullscreenStatus() const
{
    if (!m_fullscreen)
        return {};
    const CameraRuntime *runtime = nullptr;
    for (const auto &r : m_runtimes) {
        if (r.config->id == m_fullscreen->cameraId) {
            runtime = &r;
            break;
        }
    }
    return {
        { QStringLiteral("cameraId"), m_fullscreen->cameraId },
        { QStringLiteral("name"), runtime ? runtime->config->name : QString() },
        { QStringLiteral("state"), static_cast<int>(m_fullscreen->pipeline->state()) },
        { QStringLiteral("hasAudio"), m_fullscreen->pipeline->hasAudio() },
        { QStringLiteral("reconnectSeconds"), m_fullscreen->pipeline->reconnectSecondsRemaining() },
        { QStringLiteral("reconnectBackoff"), m_fullscreen->pipeline->reconnectBackoffSeconds() },
        { QStringLiteral("reconnectCount"), m_fullscreen->pipeline->reconnectCount() },
    };
}

QVariantMap CameraManager::fullscreenDiagnostics() const
{
    if (!m_fullscreen)
        return {};
    const Diagnostics d = m_fullscreen->pipeline->diagnostics();
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
