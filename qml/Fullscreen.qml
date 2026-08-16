import QtQuick

// SPEC §11/§14/§18: full available screen, video aspect unchanged.
// zoom==1.0 -> CONTAIN (letterbox allowed). zoom>1.0 -> the whole
// transformRoot is scaled around the viewport center and translated by
// pan, so it always fills the viewport with no black bars, matching
// ZOOM/COVER regardless of native video aspect ratio (SPEC §34).
Item {
    id: root

    property var info: ({})

    function refreshInfo() {
        info = cameraManager.fullscreenStatus()
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Item {
        id: transformRoot
        width: parent.width
        height: parent.height
        transformOrigin: Item.Center
        scale: navigationController.zoom
        x: navigationController.pan.x
        y: navigationController.pan.y

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
        if (navigationController.isFullscreen)
            cameraManager.attachFullscreenVideo(videoSlot)
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
    }
}
