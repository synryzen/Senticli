import QtQuick
import Senticli

Item {
    id: root
    property real openness: 1.0
    property real focusX: 0.0
    property bool warning: false

    width: 122
    height: 76

    Rectangle {
        id: sclera
        anchors.fill: parent
        radius: height / 2
        color: "#0A0C11"
        border.width: 2
        border.color: root.warning ? Colors.warning : Colors.accent
        antialiasing: true

        Rectangle {
            id: pupil
            width: 24
            height: 24
            radius: 12
            color: root.warning ? Colors.warning : Colors.accentStrong
            anchors.verticalCenter: parent.verticalCenter
            x: (parent.width - width) / 2 + root.focusX * 18
            antialiasing: true
            Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        }

        Rectangle {
            id: eyelid
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.max(0, parent.height * (1.0 - root.openness))
            radius: parent.radius
            color: Colors.panelBg
            antialiasing: true
            Behavior on height { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        }
    }
}
