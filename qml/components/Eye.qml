import QtQuick
import Senticli

Item {
    id: root
    property real openness: 1.0
    property real focusX: 0.0
    property real focusY: 0.0
    property real squint: 0.0
    property bool warning: false
    property color accentColor: root.warning ? Colors.warning : Colors.accent

    width: 122
    height: 76

    Rectangle {
        id: sclera
        width: parent.width
        height: parent.height * (1.0 - (root.squint * 0.42))
        anchors.centerIn: parent
        radius: height / 2
        color: "#0A0C11"
        border.width: 2
        border.color: root.accentColor
        antialiasing: true
        Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

        Rectangle {
            id: pupil
            width: 24
            height: 24
            radius: 12
            color: root.warning ? Colors.warning : Colors.accentStrong
            x: (parent.width - width) / 2 + root.focusX * 18
            y: (parent.height - height) / 2 + root.focusY * 8
            antialiasing: true
            Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
            Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        }

        Rectangle {
            width: 8
            height: 8
            radius: 4
            x: pupil.x + 5
            y: pupil.y + 4
            color: "#FFFFFF"
            opacity: 0.6
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
