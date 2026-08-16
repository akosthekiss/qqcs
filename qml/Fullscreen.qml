import QtQuick

// SPEC §11: full available screen, video aspect unchanged, CONTAIN at
// zoom==1.0 (letterbox/pillarbox allowed). Zoom/pan land in a later
// milestone; this is the 1.0x-only view.
Item {
    id: root

    property var info: ({})

    function refreshInfo() {
        info = cameraListModel.rowData(navigationController.fullscreenIndex)
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
    }

    function reattach() {
        if (navigationController.isFullscreen)
            cameraManager.attachFullscreenVideo(videoSlot)
    }

    Component.onCompleted: {
        reattach()
        refreshInfo()
    }

    Connections {
        // Deliberately targets cameraManager, not navigationController:
        // cameraManager.fullscreenIdChanged is emitted only once its own
        // pipeline switch is fully done, so reattaching here can never race
        // against pipeline creation -- unlike navigationController's
        // fullscreenCameraActivated, whose delivery order relative to
        // CameraManager::switchFullscreenCamera (a separate connection on a
        // separate object) is not guaranteed.
        target: cameraManager
        function onFullscreenIdChanged(id) { root.reattach() }
    }

    Connections {
        target: navigationController
        function onFullscreenIndexChanged() { root.refreshInfo() }
    }

    Connections {
        target: cameraListModel
        function onDataChanged() { root.refreshInfo() }
    }
}
