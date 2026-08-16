import QtQuick
import QtQuick.Window

Window {
    width: 1280
    height: 720
    visible: true
    title: "QQCS"

    Rectangle {
        anchors.fill: parent
        color: "black"

        Item {
            id: videoSlot
            anchors.fill: parent
            Component.onCompleted: cameraManager.attachMosaicVideo(cameraManager.firstCameraId(), videoSlot)
        }
    }
}
