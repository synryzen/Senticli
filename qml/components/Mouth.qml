import QtQuick
import Senticli

Item {
    id: root
    property string stateName: "idle"
    property bool speaking: false
    property int visemeFrame: 0

    width: 180
    height: 62

    property real speakingOpen: [14, 24, 18, 30, 16, 22][visemeFrame % 6]
    property real speakingWidth: [124, 138, 112, 146, 118, 132][visemeFrame % 6]
    property real speakingTilt: [-2, 1, -1, 2, 0, -1][visemeFrame % 6]

    property real baseHeight: {
        if (stateName === "happy") return 16
        if (stateName === "confused") return 9
        if (stateName === "warning") return 7
        return 12
    }

    property real baseWidth: {
        if (stateName === "confused") return 102
        if (stateName === "happy") return 136
        return 128
    }

    Timer {
        id: visemeTimer
        interval: 86
        running: root.speaking
        repeat: true
        onTriggered: root.visemeFrame = (root.visemeFrame + 1) % 12
    }

    onSpeakingChanged: {
        if (!speaking) {
            visemeFrame = 0
        }
    }

    Rectangle {
        id: mouth
        width: root.speaking ? root.speakingWidth : root.baseWidth
        height: root.speaking ? root.speakingOpen : root.baseHeight
        radius: height / 2
        anchors.centerIn: parent
        color: "transparent"
        rotation: root.speaking ? root.speakingTilt : (root.stateName === "confused" ? -4 : 0)
        border.width: root.speaking ? 2 : 3
        border.color: stateName === "warning" ? Colors.warning : Colors.accent
        antialiasing: true

        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 14
            height: Math.max(4, parent.height - 10)
            radius: height / 2
            color: root.stateName === "warning" ? "#4A201C" : "#1C0F0B"
            opacity: root.speaking ? 0.9 : 0.65
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            width: root.speaking ? parent.width * 0.42 : 0
            height: root.speaking ? Math.max(3, parent.height * 0.25) : 0
            radius: height / 2
            color: "#C66A5D"
            opacity: 0.75
        }

        Behavior on width { NumberAnimation { duration: root.speaking ? 70 : 170; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: root.speaking ? 70 : 170; easing.type: Easing.OutCubic } }
        Behavior on rotation { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 170 } }
    }
}
