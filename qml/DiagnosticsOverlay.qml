// Copyright (c) 2026 Akos Kiss.
//
// Licensed under the BSD 3-Clause License
// <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
// This file may not be copied, modified, or distributed except
// according to those terms.

import QtQuick

// SPEC §22: toggleable overlay (Blue CEC / "I" key), not a separate
// window. Every field falls back to "N/A" when unavailable, per spec.
// Polling here is a periodic *redraw* of live values (bitrate, fps,
// dropped frames) already read straight from the pipeline each tick --
// it is not a second, independent reconnect countdown (that constraint,
// SPEC §20.2, is about the specific countdown value's source, not about
// whether a QML Timer may exist at all).
Item {
    id: root

    property var info: ({})
    visible: navigationController.diagnosticsVisible

    function naOr(value) {
        return (value === undefined || value === null || value === "") ? "N/A" : String(value)
    }

    function refresh() {
        info = cameraManager.fullscreenDiagnostics()
    }

    Component.onCompleted: refresh()

    Timer {
        interval: 500
        running: root.visible
        repeat: true
        onTriggered: root.refresh()
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        width: column.width + 24
        height: column.height + 20
        color: "black"
        opacity: 0.72
        radius: 6

        Column {
            id: column
            anchors.centerIn: parent
            spacing: 3

            Text { color: "white"; font.pixelSize: 15; font.bold: true; text: "DIAGNOSTICS" }
            Text { color: "#dddddd"; font.pixelSize: 13; text: "Codec: " + root.naOr(root.info.videoCodec) }
            Text {
                color: "#dddddd"; font.pixelSize: 13
                text: "Resolution: " + (root.info.width > 0 ? (root.info.width + "x" + root.info.height) : "N/A")
            }
            Text {
                color: "#dddddd"; font.pixelSize: 13
                text: "FPS: " + (root.info.fps > 0 ? root.info.fps.toFixed(1) : "N/A")
            }
            Text {
                color: "#dddddd"; font.pixelSize: 13
                text: "Bitrate: " + (root.info.bitrateBps > 0 ? (Math.round(root.info.bitrateBps / 1000) + " kbps") : "N/A")
            }
            Text { color: "#dddddd"; font.pixelSize: 13; text: "Audio codec: " + root.naOr(root.info.audioCodec) }
            Text { color: "#dddddd"; font.pixelSize: 13; text: "RTSP transport: " + root.naOr(root.info.rtspTransport) }
            Text {
                color: "#dddddd"; font.pixelSize: 13
                text: "Dropped frames: " + (root.info.droppedFrames !== undefined ? root.info.droppedFrames : "N/A")
            }
            Text {
                color: "#dddddd"; font.pixelSize: 13
                text: "Reconnect count: " + (root.info.reconnectCount !== undefined ? root.info.reconnectCount : "N/A")
            }
            Text {
                color: "#dddddd"; font.pixelSize: 13
                text: "Reconnect backoff: "
                      + (root.info.reconnectBackoffSeconds > 0 ? (root.info.reconnectBackoffSeconds + "s") : "N/A")
            }
            Text {
                color: "#dddddd"; font.pixelSize: 13
                text: "Reconnect countdown: "
                      + (root.info.reconnectCountdownSeconds > 0 ? (root.info.reconnectCountdownSeconds + "s") : "N/A")
            }
            Text { color: "#dddddd"; font.pixelSize: 13; text: "RTSP URL: " + root.naOr(root.info.rtspUrl) }
        }
    }
}
