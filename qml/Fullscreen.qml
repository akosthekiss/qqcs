// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

import QtQuick

// SPEC §11/§14/§18/§34: full available screen, video aspect unchanged.
// zoom==1.0 -> CONTAIN (letterbox allowed). zoom>1.0 -> ZOOM/COVER:
// transformRoot is sized to the video's own CONTAIN-letterboxed content
// rect (cameraManager.fullscreenContentSize), not the full window/parent,
// so any black bar is static background OUTSIDE the scaled/panned item --
// scaling transformRoot up therefore genuinely shrinks the bars (they
// never move/grow with it) until they're fully covered, rather than just
// scaling a fixed letterboxed image (bars included) uniformly, which
// would keep them reachable via pan at any zoom. `clip: true` is needed
// since transformRoot can grow past root's own bounds once zoomed.
Item {
    id: root
    clip: true

    property var info: ({})

    function refreshInfo() {
        info = cameraManager.fullscreenStatus()
    }

    // cameraManager.fullscreenContentSize() is Q_INVOKABLE, not a
    // NOTIFYing Q_PROPERTY -- QML can't track it as a binding dependency,
    // so this is called imperatively from the actual triggers (window
    // resize, camera switch, and CameraManager's own
    // fullscreenContentSizeChanged, fired once native resolution becomes
    // known) rather than as a declarative width/height binding.
    function syncContentSize() {
        const size = cameraManager.fullscreenContentSize()
        const valid = size.width > 0 && size.height > 0
        transformRoot.width = valid ? size.width : root.width
        transformRoot.height = valid ? size.height : root.height
        navigationController.setContentSize(Qt.size(transformRoot.width, transformRoot.height))
    }

    function updateAvailableSize() {
        cameraManager.setFullscreenAvailableSize(Qt.size(root.width, root.height))
        syncContentSize()
    }

    onWidthChanged: updateAvailableSize()
    onHeightChanged: updateAvailableSize()

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Item {
        id: transformRoot
        transformOrigin: Item.Center
        scale: navigationController.zoom
        // Centered within root by default (pan==0), same as the
        // pre-§34 letterboxed look; pan then offsets from there.
        x: (root.width - width) / 2 + navigationController.pan.x
        y: (root.height - height) / 2 + navigationController.pan.y

        Item {
            id: videoSlot
            anchors.fill: parent
        }
    }

    MouseArea {
        id: panArea
        anchors.fill: parent
        property point lastPos

        onWheel: (wheel) => navigationController.handleWheelZoom(wheel.angleDelta.y, Qt.point(wheel.x, wheel.y))

        onPressed: (mouse) => { lastPos = Qt.point(mouse.x, mouse.y) }

        // SPEC §19: left-button-drag pans only while zoomed in; at
        // zoom==1.0 this is simply inert (no camera-switch side effect --
        // that's Left/Right key input, a separate InputAction path).
        onPositionChanged: (mouse) => {
            if (!pressed || navigationController.zoom <= 1.0)
                return
            const delta = Qt.point(mouse.x - lastPos.x, mouse.y - lastPos.y)
            navigationController.handlePanDragDelta(delta)
            lastPos = Qt.point(mouse.x, mouse.y)
        }
    }

    StatusOverlay {
        anchors.fill: parent
        cameraName: root.info.name || ""
        cameraState: root.info.state !== undefined ? root.info.state : 0
        hasAudio: root.info.hasAudio || false
        reconnectSeconds: root.info.reconnectSeconds !== undefined ? root.info.reconnectSeconds : -1
        showAudioStatus: true // SPEC: audio status only meaningful where audio actually plays
    }

    DiagnosticsOverlay {
        anchors.fill: parent
    }

    function reattach() {
        if (navigationController.isFullscreen) {
            cameraManager.attachFullscreenVideo(videoSlot)
            updateAvailableSize() // new video item -> needs its available size (re-)set
        }
    }

    Component.onCompleted: {
        reattach()
        refreshInfo()
    }

    // Deliberately targets cameraManager, not navigationController:
    // cameraManager.fullscreenIdChanged/fullscreenStatusChanged are emitted
    // only once its own pipeline switch is fully done, so reacting here can
    // never race against pipeline creation -- unlike navigationController's
    // fullscreenCameraActivated, whose delivery order relative to
    // CameraManager::switchFullscreenCamera (a separate connection on a
    // separate object) is not guaranteed.
    Connections {
        target: cameraManager
        function onFullscreenIdChanged(id) { root.reattach() }
        function onFullscreenStatusChanged() { root.refreshInfo() }
        // Fires once the new camera's native resolution becomes known
        // (first frame decoded) -- until then syncContentSize() (already
        // called from reattach()) falls back to the full window size.
        function onFullscreenContentSizeChanged() { root.syncContentSize() }
    }
}
