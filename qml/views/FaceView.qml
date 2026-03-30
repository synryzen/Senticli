import QtQuick
import QtQuick.Layouts
import Senticli

Item {
    id: root
    property string faceState: "idle"
    property string statusText: "Ready"
    property bool micActive: false
    property bool speakingActive: false

    property real blinkFactor: 1.0
    property real driftX: 0.0
    property real moodSwing: 0.0
    property real idleBob: 0.0
    property real talkBeat: 0.0
    property bool isWarning: root.faceState === "warning"
    property bool isThinking: root.faceState === "thinking"
    property bool isListening: root.faceState === "listening" || root.micActive
    property bool isSpeaking: root.speakingActive
    property bool isHappy: root.faceState === "happy"
    property bool isConfused: root.faceState === "confused"
    property color expressionAccent: {
        if (root.isWarning) return Colors.warning
        if (root.isSpeaking) return Colors.info
        if (root.isHappy) return Colors.success
        return Colors.accent
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#151A21" }
            GradientStop { position: 1.0; color: Colors.windowBg }
        }
    }

    Rectangle {
        id: glow
        width: 380
        height: 380
        radius: width / 2
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        color: "transparent"
        border.width: 2
        border.color: root.expressionAccent
        opacity: root.isListening ? 0.92 : (root.isSpeaking ? 0.82 : 0.52)

        SequentialAnimation on scale {
            loops: Animation.Infinite
            running: true
            NumberAnimation { from: 0.97; to: 1.035; duration: root.isSpeaking ? 520 : 880; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 1.035; to: 0.97; duration: root.isSpeaking ? 520 : 880; easing.type: Easing.InOutQuad }
        }
    }

    Rectangle {
        id: facePlate
        width: 520
        height: 250
        radius: 28
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        color: Colors.panelBg
        border.width: 1
        border.color: root.expressionAccent
        y: (root.isSpeaking ? (-6 + root.talkBeat) : (root.isListening ? -2 : 0)) + root.idleBob
        scale: root.isSpeaking ? (1.014 + root.talkBeat * 0.004) : 1.0
        rotation: root.isConfused ? -1.8 : (root.isHappy ? 0.8 : 0)

        Behavior on y { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 180 } }

        Column {
            anchors.centerIn: parent
            spacing: 26

            Row {
                spacing: 106
                anchors.horizontalCenter: parent.horizontalCenter

                Rectangle {
                    width: 78
                    height: 7
                    radius: 4
                    color: root.expressionAccent
                    opacity: 0.9
                    rotation: root.isConfused ? -22 : (root.isWarning ? -14 : (root.isHappy ? -16 : (-3 + root.moodSwing * 6 + root.talkBeat * 2.5)))
                    y: root.isThinking ? -4 : 0
                    Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                }

                Rectangle {
                    width: 78
                    height: 7
                    radius: 4
                    color: root.expressionAccent
                    opacity: 0.9
                    rotation: root.isConfused ? 10 : (root.isWarning ? 18 : (root.isHappy ? 16 : (3 - root.moodSwing * 6 - root.talkBeat * 2.5)))
                    y: root.isThinking ? -6 : 0
                    Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                }
            }

            Row {
                spacing: 72
                anchors.horizontalCenter: parent.horizontalCenter

                Eye {
                    openness: (root.isWarning ? 0.48 : (root.isThinking ? 0.64 : (root.isHappy ? 0.9 : 0.94))) * root.blinkFactor
                    focusX: root.isThinking ? -0.34 : (root.isListening ? 0.22 : (root.driftX + root.moodSwing * 0.14))
                    focusY: root.isThinking ? -0.2 : (root.isSpeaking ? (0.07 + root.talkBeat * 0.11) : (root.isHappy ? -0.04 : 0.0))
                    squint: root.isWarning ? 0.58 : (root.isSpeaking ? 0.2 : (root.isHappy ? 0.08 : 0.0))
                    pupilScale: root.isWarning ? 0.86 : (root.isSpeaking ? 1.08 : 1.0)
                    glintOpacity: root.isWarning ? 0.35 : (root.isHappy ? 0.85 : 0.62)
                    warning: root.isWarning
                    accentColor: root.expressionAccent
                }

                Eye {
                    openness: (root.isWarning ? 0.48 : (root.isThinking ? 0.64 : (root.isHappy ? 0.9 : 0.94))) * root.blinkFactor
                    focusX: root.isThinking ? 0.34 : (root.isListening ? -0.22 : (-root.driftX + root.moodSwing * 0.14))
                    focusY: root.isThinking ? -0.2 : (root.isSpeaking ? (0.07 + root.talkBeat * 0.11) : (root.isHappy ? -0.04 : 0.0))
                    squint: root.isWarning ? 0.58 : (root.isSpeaking ? 0.2 : (root.isHappy ? 0.08 : 0.0))
                    pupilScale: root.isWarning ? 0.86 : (root.isSpeaking ? 1.08 : 1.0)
                    glintOpacity: root.isWarning ? 0.35 : (root.isHappy ? 0.85 : 0.62)
                    warning: root.isWarning
                    accentColor: root.expressionAccent
                }
            }

            Mouth {
                anchors.horizontalCenter: parent.horizontalCenter
                stateName: root.faceState
                speaking: root.isSpeaking
            }
        }

        Rectangle {
            width: 24
            height: 12
            radius: 8
            color: root.isHappy ? "#6AE6B1" : (root.isWarning ? "#F1A08D" : "#FFFFFF")
            opacity: root.isHappy ? 0.22 : (root.isWarning ? 0.18 : 0.0)
            anchors.left: parent.left
            anchors.leftMargin: 112
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 56
        }

        Rectangle {
            width: 24
            height: 12
            radius: 8
            color: root.isHappy ? "#6AE6B1" : (root.isWarning ? "#F1A08D" : "#FFFFFF")
            opacity: root.isHappy ? 0.22 : (root.isWarning ? 0.18 : 0.0)
            anchors.right: parent.right
            anchors.rightMargin: 112
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 56
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: facePlate.bottom
        anchors.topMargin: 14
        text: root.statusText
        color: Colors.textSecondary
        font.family: Typography.uiFamily
        font.pixelSize: Typography.bodySize
    }

    Timer {
        id: blinkTimer
        interval: 2200
        running: true
        repeat: true
        onTriggered: {
            blink.restart()
            blinkTimer.interval = 1500 + Math.random() * 1800
        }
    }

    Timer {
        interval: 1700
        running: true
        repeat: true
        onTriggered: {
            if (root.isThinking || root.isListening || root.isWarning) {
                root.driftX = 0
                return
            }
            root.driftX = (Math.random() * 0.9) - 0.45
        }
    }

    SequentialAnimation on moodSwing {
        loops: Animation.Infinite
        running: true
        NumberAnimation { from: -1.0; to: 1.0; duration: root.isSpeaking ? 620 : 1200; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.0; to: -1.0; duration: root.isSpeaking ? 620 : 1200; easing.type: Easing.InOutSine }
    }

    SequentialAnimation on idleBob {
        loops: Animation.Infinite
        running: !root.isSpeaking
        NumberAnimation { from: -1.0; to: 1.3; duration: 1500; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.3; to: -1.0; duration: 1500; easing.type: Easing.InOutSine }
    }

    SequentialAnimation on talkBeat {
        loops: Animation.Infinite
        running: root.isSpeaking
        NumberAnimation { from: -1.0; to: 1.0; duration: 130; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.0; to: -1.0; duration: 130; easing.type: Easing.InOutSine }
    }

    SequentialAnimation {
        id: blink
        NumberAnimation { target: root; property: "blinkFactor"; to: 0.12; duration: 80; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root; property: "blinkFactor"; to: 1.0; duration: 110; easing.type: Easing.InOutQuad }
    }
}
