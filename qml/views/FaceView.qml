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
        border.color: root.faceState === "warning" ? Colors.warning : Colors.accentMuted
        opacity: root.micActive ? 0.9 : 0.45

        SequentialAnimation on scale {
            loops: Animation.Infinite
            running: root.micActive
            NumberAnimation { from: 0.97; to: 1.03; duration: 880; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 1.03; to: 0.97; duration: 880; easing.type: Easing.InOutQuad }
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
        border.color: Colors.border

        Column {
            anchors.centerIn: parent
            spacing: 34

            Row {
                spacing: 72
                anchors.horizontalCenter: parent.horizontalCenter

                Eye {
                    openness: (root.faceState === "thinking" ? 0.72 : 0.95) * root.blinkFactor
                    focusX: root.faceState === "thinking" ? -0.4 : (root.faceState === "listening" ? 0.2 : 0.0)
                    warning: root.faceState === "warning"
                }

                Eye {
                    openness: (root.faceState === "thinking" ? 0.72 : 0.95) * root.blinkFactor
                    focusX: root.faceState === "thinking" ? 0.4 : (root.faceState === "listening" ? -0.2 : 0.0)
                    warning: root.faceState === "warning"
                }
            }

            Mouth {
                anchors.horizontalCenter: parent.horizontalCenter
                stateName: root.faceState
                speaking: root.speakingActive
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

    SequentialAnimation {
        id: blink
        NumberAnimation { target: root; property: "blinkFactor"; to: 0.12; duration: 80; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root; property: "blinkFactor"; to: 1.0; duration: 110; easing.type: Easing.InOutQuad }
    }
}
