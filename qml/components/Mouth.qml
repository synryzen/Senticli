import QtQuick
import Senticli

Item {
    id: root
    property string stateName: "idle"
    property string styleName: "Loona"
    property bool speaking: false
    property int beatMs: 180
    property int visemeFrame: 0

    width: 190
    height: 70

    readonly property bool styleHappy: stateName === "happy"
    readonly property bool styleWarning: stateName === "warning"
    readonly property bool styleAngry: stateName === "angry"
    readonly property bool styleSad: stateName === "sad"
    readonly property bool styleSleeping: stateName === "sleeping"
    readonly property bool styleConfused: stateName === "confused"
    readonly property bool styleThinking: stateName === "thinking"
    readonly property bool styleListening: stateName === "listening"

    readonly property var openFrames: root.styleName === "Loona"
                                      ? [8, 14, 20, 12, 18, 10, 16, 13]
                                      : (root.styleName === "Orb"
                                         ? [7, 16, 11, 18, 9, 14, 11, 20]
                                         : [10, 24, 16, 30, 14, 20, 18, 26])
    readonly property var widthFrames: root.styleName === "Loona"
                                       ? [104, 118, 132, 112, 126, 110, 122, 116]
                                       : (root.styleName === "Orb"
                                          ? [98, 116, 104, 122, 106, 120, 108, 124]
                                          : [114, 136, 106, 144, 116, 132, 120, 138])
    readonly property var tiltFrames: root.styleName === "Terminal"
                                      ? [-2, 2, -1, 1, 0, -1, 2, -2]
                                      : [-1, 1, 0, 1, 0, -1, 1, -1]

    property real speakingOpen: openFrames[visemeFrame % 8]
    property real speakingWidth: widthFrames[visemeFrame % 8]
    property real speakingTilt: tiltFrames[visemeFrame % 8]

    Timer {
        id: visemeTimer
        interval: root.beatMs
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
            visible: !root.styleHappy && !root.styleListening && !root.styleConfused && !root.styleSad && !root.styleSleeping && !root.styleAngry
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
            visible: root.styleWarning || root.styleAngry

            Rectangle {
                width: 148
                height: 148
                radius: 74
                x: 0
                y: -2
                color: "transparent"
                border.width: 4
                border.color: root.styleAngry ? "#FF6A4C" : Colors.warning
            }
        }

        Item {
            anchors.centerIn: parent
            width: 160
            height: 52
            clip: true
            visible: root.styleSad

            Rectangle {
                width: 160
                height: 160
                radius: 80
                x: 0
                y: 96
                color: "transparent"
                border.width: 4
                border.color: "#7AA7FF"
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: root.styleSleeping ? 62 : 0
            height: root.styleSleeping ? 8 : 0
            radius: 4
            color: "#89A0B8"
            visible: root.styleSleeping
            opacity: 0.85
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
        Behavior on height { NumberAnimation { duration: Math.max(58, Math.min(110, root.beatMs / 2)); easing.type: Easing.OutCubic } }
        Behavior on rotation { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
    }
}
