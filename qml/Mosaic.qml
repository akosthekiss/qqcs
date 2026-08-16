import QtQuick

// SPEC §8/§9: grid of every configured camera, COVER fill (crop allowed,
// never distorted, never letterboxed). Column count from layout.columns;
// row count derived automatically. Focus handling and navigation land in
// a later milestone.
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

            width: root.width / root.columns
            height: root.rowCount > 0 ? root.height / root.rowCount : root.height
            x: (index % root.columns) * width
            y: Math.floor(index / root.columns) * height

            CameraTile {
                anchors.fill: parent
                cameraId: cell.cameraId
            }
        }
    }
}
