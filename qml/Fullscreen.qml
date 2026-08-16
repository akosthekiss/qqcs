import QtQuick

// SPEC §11: full available screen, video aspect unchanged, CONTAIN at
// zoom==1.0 (letterbox/pillarbox allowed). Zoom/pan land in a later
// milestone; this is the 1.0x-only view.
Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Item {
        id: videoSlot
        anchors.fill: parent
    }

    function reattach() {
        if (navigationController.isFullscreen)
            cameraManager.attachFullscreenVideo(videoSlot)
    }

    Component.onCompleted: reattach()

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
}
