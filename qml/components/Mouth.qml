import QtQuick
import Senticli

Item {
    id: root
    property string stateName: "idle"
    property bool speaking: false
    property int visemeFrame: 0

    width: 190
    height: 70

    readonly property bool styleHappy: stateName === "happy"
    readonly property bool styleWarning: stateName === "warning"
    readonly property bool styleConfused: stateName === "confused"
    readonly property bool styleThinking: stateName === "thinking"
    readonly property bool styleListening: stateName === "listening"

    property real speakingOpen: [12, 28, 18, 34, 16, 24, 20, 30][visemeFrame % 8]
    property real speakingWidth: [118, 142, 108, 150, 120, 136, 124, 146][visemeFrame % 8]
    property real speakingTilt: [-2, 2, -1, 1, 0, -1, 2, -2][visemeFrame % 8]

    Timer {
        id: visemeTimer
        interval: 80
        running: root.speaking
        repeat: true
        onTriggered: root.visemeFrame = (root.visemeFrame + 1) % 16
    }

    onSpeakingChanged: {
        if (!speaking) {
            visemeFrame = 0
        }
    }

    Item {
        anchors.fill: parent
        visible: !root.speaking

        Rectangle {
            anchors.centerIn: parent
            width: 134
            height: 12
            radius: 6
            color: root.styleWarning ? Colors.warning : Colors.accent
            visible: !root.styleHappy && !root.styleListening && !root.styleConfused
            opacity: root.styleThinking ? 0.75 : 0.95
        }

        Item {
            anchors.centerIn: parent
            width: 160
            height: 56
            clip: true
            visible: root.styleHappy

            Rectangle {
                width: 160
                height: 160
                radius: 80
                x: 0
                y: -116
                color: "transparent"
                border.width: 5
                border.color: Colors.success
            }
        }

        Item {
            anchors.centerIn: parent
            width: 148
            height: 46
            clip: true
            visible: root.styleWarning

            Rectangle {
                width: 148
                height: 148
                radius: 74
                x: 0
                y: -2
                color: "transparent"
                border.width: 4
                border.color: Colors.warning
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.styleListening ? 34 : 0
            height: root.styleListening ? 28 : 0
            radius: height / 2
            color: "transparent"
            border.width: 4
            border.color: Colors.info
            visible: root.styleListening
        }

        Text {
            anchors.centerIn: parent
            visible: root.styleConfused
            text: "~  ~"
            color: Colors.warning
            font.family: Typography.monoFamily
            font.pixelSize: 28
            font.bold: true
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            visible: root.styleThinking

            Repeater {
                model: 3
                delegate: Rectangle {
                    width: 7
                    height: 7
                    radius: 4
                    color: Colors.accent
                    opacity: 0.75 - (index * 0.2)
                }
            }
        }
    }

    Rectangle {
        id: speakingMouth
        visible: root.speaking
        width: root.speakingWidth
        height: root.speakingOpen
        radius: height / 2
        anchors.centerIn: parent
        color: "transparent"
        border.width: 2
        border.color: Colors.info
        rotation: root.speakingTilt
        antialiasing: true

        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 14
            height: Math.max(4, parent.height - 9)
            radius: height / 2
            color: "#1B0E15"
            opacity: 0.95
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 3
            width: parent.width * 0.42
            height: Math.max(3, parent.height * 0.26)
            radius: height / 2
            color: "#DE7D7D"
            opacity: 0.82
        }

        Behavior on width { NumberAnimation { duration: 66; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: 66; easing.type: Easing.OutCubic } }
        Behavior on rotation { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
    }
}
