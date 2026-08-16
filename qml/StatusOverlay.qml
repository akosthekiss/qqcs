import QtQuick

// SPEC §20/§20.1/§20.2: always-on overlay, readable from across a room.
// Content: camera name (if any), LIVE/LOST, audio *capability* (not
// playback state), and while LOST the reconnect countdown -- which must
// come from the real reconnect scheduler (a later milestone), never an
// independent UI timer, so this component only ever displays
// reconnectSeconds as given, it never counts down on its own.
Item {
    id: root

    property string cameraName: ""
    property int cameraState: 0 // CameraState enum ordinal: Disconnected/Connecting/Live/Lost
    property bool hasAudio: false
    property int reconnectSeconds: -1

    readonly property bool isConnecting: cameraState === 1
    readonly property bool isLive: cameraState === 2
    readonly property bool isLost: cameraState === 3
    readonly property string stateText: isLive ? "LIVE" : (isLost ? "LOST" : (isConnecting ? "CONNECTING" : "DISCONNECTED"))
    readonly property color stateColor: isLive ? "#3ddc55" : (isLost ? "#e0463b" : "#c9c9c9")
    readonly property bool showName: configModel.overlay.showName && cameraName.length > 0
    readonly property bool showStatusLine: configModel.overlay.showStatus

    visible: configModel.overlay.enabled && (showName || showStatusLine)

    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        width: column.width + 16
        height: column.height + 10
        color: "black"
        opacity: 0.55
        radius: 4

        Column {
            id: column
            anchors.centerIn: parent
            spacing: 2

            Text {
                visible: root.showStatusLine
                color: root.stateColor
                font.pixelSize: 18
                font.bold: true
                text: "● " + root.stateText + "  " + (root.hasAudio ? "🔊" : "🔇")
                      + (root.showName ? "  " + root.cameraName.toUpperCase() : "")
            }

            Text {
                visible: !root.showStatusLine && root.showName
                color: "white"
                font.pixelSize: 18
                font.bold: true
                text: root.cameraName.toUpperCase()
            }

            Text {
                visible: root.isLost && root.reconnectSeconds > 0
                color: "#e0463b"
                font.pixelSize: 15
                text: "Reconnect: " + root.reconnectSeconds + "s"
            }
        }
    }
}
