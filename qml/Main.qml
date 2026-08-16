import QtQuick
import QtQuick.Window

Window {
    id: window
    width: 1280
    height: 720
    visible: true
    title: "QQCS"

    onWidthChanged: navigationController.setViewportSize(Qt.size(width, height))
    onHeightChanged: navigationController.setViewportSize(Qt.size(width, height))
    Component.onCompleted: navigationController.setViewportSize(Qt.size(width, height))

    Rectangle {
        anchors.fill: parent
        color: "black"

        Mosaic {
            anchors.fill: parent
            visible: !navigationController.isFullscreen
        }

        Fullscreen {
            anchors.fill: parent
            visible: navigationController.isFullscreen
        }
    }
}
