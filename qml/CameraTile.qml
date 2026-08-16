import QtQuick

Item {
    id: tile

    required property string cameraId
    property int tileIndex: -1
    property string cameraName: ""
    property int cameraState: 0
    property bool hasAudio: false
    property int reconnectSeconds: -1

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Item {
        id: videoSlot
        anchors.fill: parent
        Component.onCompleted: cameraManager.attachMosaicVideo(tile.cameraId, videoSlot)
    }

    StatusOverlay {
        anchors.fill: parent
        cameraName: tile.cameraName
        cameraState: tile.cameraState
        hasAudio: tile.hasAudio
        reconnectSeconds: tile.reconnectSeconds
    }

    MouseArea {
        anchors.fill: parent
        onClicked: navigationController.selectMosaicTile(tile.tileIndex) // SPEC §11: left-click enters fullscreen
    }
}
