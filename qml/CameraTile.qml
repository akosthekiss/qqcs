import QtQuick

Item {
    id: tile

    required property string cameraId
    property int tileIndex: -1

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Item {
        id: videoSlot
        anchors.fill: parent
        Component.onCompleted: cameraManager.attachMosaicVideo(tile.cameraId, videoSlot)
    }

    MouseArea {
        anchors.fill: parent
        onClicked: navigationController.selectMosaicTile(tile.tileIndex) // SPEC §11: left-click enters fullscreen
    }
}
