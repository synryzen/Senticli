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
        opacity: root.isListening ? 0.9 : (root.isSpeaking ? 0.75 : 0.45)

        SequentialAnimation on scale {
            loops: Animation.Infinite
            running: root.isListening || root.isSpeaking
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
        y: root.isSpeaking ? -2 : 0

        Behavior on y { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
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
                    rotation: root.isConfused ? -15 : (root.isWarning ? -6 : (root.isHappy ? -10 : -2))
                    y: root.isThinking ? -4 : 0
                    Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                }

                Rectangle {
                    width: 78
                    height: 7
                    radius: 4
                    color: root.expressionAccent
                    opacity: 0.9
                    rotation: root.isConfused ? 6 : (root.isWarning ? 12 : (root.isHappy ? 10 : 2))
                    y: root.isThinking ? -6 : 0
                    Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                }
            }

            Row {
                spacing: 72
                anchors.horizontalCenter: parent.horizontalCenter

                Eye {
                    openness: (root.isWarning ? 0.56 : (root.isThinking ? 0.66 : (root.isHappy ? 0.92 : 0.96))) * root.blinkFactor
                    focusX: root.isThinking ? -0.32 : (root.isListening ? 0.18 : root.driftX)
                    focusY: root.isThinking ? -0.16 : (root.isSpeaking ? 0.05 : 0.0)
                    squint: root.isWarning ? 0.48 : (root.isSpeaking ? 0.14 : 0.0)
                    warning: root.isWarning
                    accentColor: root.expressionAccent
                }

                Eye {
                    openness: (root.isWarning ? 0.56 : (root.isThinking ? 0.66 : (root.isHappy ? 0.92 : 0.96))) * root.blinkFactor
                    focusX: root.isThinking ? 0.32 : (root.isListening ? -0.18 : -root.driftX)
                    focusY: root.isThinking ? -0.16 : (root.isSpeaking ? 0.05 : 0.0)
                    squint: root.isWarning ? 0.48 : (root.isSpeaking ? 0.14 : 0.0)
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
        interval: 3200
        running: true
        repeat: true
        onTriggered: blink.restart()
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
            root.driftX = (Math.random() * 0.5) - 0.25
        }
    }

    SequentialAnimation {
        id: blink
        NumberAnimation { target: root; property: "blinkFactor"; to: 0.12; duration: 80; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root; property: "blinkFactor"; to: 1.0; duration: 110; easing.type: Easing.InOutQuad }
    }
}
