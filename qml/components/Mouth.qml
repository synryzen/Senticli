import QtQuick
import Senticli

Item {
    id: root
    property string stateName: "idle"
    property bool speaking: false

    width: 180
    height: 62

    property real mouthHeight: {
        if (speaking) return 30
        if (stateName === "happy") return 18
        if (stateName === "confused") return 8
        if (stateName === "warning") return 7
        return 12
    }

    Rectangle {
        id: mouth
        width: stateName === "confused" ? 100 : 132
        height: root.mouthHeight
        radius: height / 2
        anchors.centerIn: parent
        color: "transparent"
        border.width: 3
        border.color: stateName === "warning" ? Colors.warning : Colors.accent
        antialiasing: true

        Behavior on width { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 170 } }
    }
}
