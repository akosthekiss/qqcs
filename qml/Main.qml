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
        id: root
        anchors.fill: parent
        color: "black"

        // Sole place that touches raw key events (SPEC §39: no business
        // logic in QML) -- InputManager maps them to InputAction, which
        // NavigationController alone interprets based on current state.
        focus: true
        Keys.onPressed: (event) => inputManager.handleKeyEvent(event.key)

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
