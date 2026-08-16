import QtQuick

// SPEC §8/§9/§10: grid of every configured camera, COVER fill (crop
// allowed, never distorted, never letterboxed). Column count from
// layout.columns; row count derived automatically. Focused tile is
// whichever index NavigationController currently holds.
Item {
    id: root

    property int columns: Math.max(1, configModel.columns)
    property int rowCount: repeater.count > 0 ? Math.ceil(repeater.count / columns) : 0

    Repeater {
        id: repeater
        model: cameraListModel

        delegate: Item {
            id: cell
            required property int index
            required property string cameraId
            required property string name
            required property int state
            required property bool hasAudio
            required property int reconnectSeconds

            width: root.width / root.columns
            height: root.rowCount > 0 ? root.height / root.rowCount : root.height
            x: (index % root.columns) * width
            y: Math.floor(index / root.columns) * height

            CameraTile {
                anchors.fill: parent
                cameraId: cell.cameraId
                tileIndex: cell.index
                cameraName: cell.name
                cameraState: cell.state
                hasAudio: cell.hasAudio
                reconnectSeconds: cell.reconnectSeconds
                isFocused: cell.index === navigationController.focusedMosaicIndex
            }
        }
    }
}
