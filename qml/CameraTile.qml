import QtQuick

Item {
    id: tile

    required property string cameraId

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Item {
        id: videoSlot
        anchors.fill: parent
        Component.onCompleted: cameraManager.attachMosaicVideo(tile.cameraId, videoSlot)
    }
}
