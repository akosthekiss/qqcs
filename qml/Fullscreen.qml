import QtQuick

// SPEC §11: full available screen, video aspect unchanged, CONTAIN at
// zoom==1.0 (letterbox/pillarbox allowed). Zoom/pan land in a later
// milestone; this is the 1.0x-only view.
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
        id: videoSlot
        anchors.fill: parent
    }

    StatusOverlay {
        anchors.fill: parent
        cameraName: root.info.name || ""
        cameraState: root.info.state !== undefined ? root.info.state : 0
        hasAudio: root.info.hasAudio || false
        reconnectSeconds: root.info.reconnectSeconds !== undefined ? root.info.reconnectSeconds : -1
        showAudioStatus: true // SPEC: audio status only meaningful where audio actually plays
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
