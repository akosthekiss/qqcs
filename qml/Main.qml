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

        Mosaic {
            anchors.fill: parent
        }
    }
}
