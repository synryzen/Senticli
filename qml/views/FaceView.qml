import QtQuick
import QtQuick.Layouts
import Senticli

Item {
    id: root
    property string faceState: "idle"
    property string statusText: "Ready"
    property bool micActive: false
    property real voiceInputLevel: 0.0
    property string faceStyle: "Loona"
    property int mouthBeatMs: 180
    property bool speakingActive: false

    property real blinkFactor: 1.0
    property real driftX: 0.0
    property real moodSwing: 0.0
    property real idleBob: 0.0
    property real talkBeat: 0.0
    property bool sleepMode: false
    readonly property bool styleLoona: root.faceStyle === "Loona"
    readonly property bool styleTerminal: root.faceStyle === "Terminal"
    readonly property bool styleOrb: root.faceStyle === "Orb"
    readonly property string effectiveState: root.sleepMode ? "sleeping" : root.faceState
    property bool isWarning: root.effectiveState === "warning"
    property bool isThinking: root.effectiveState === "thinking"
    property bool isListening: root.effectiveState === "listening" || root.micActive
    property bool isSpeaking: root.speakingActive
    property bool isHappy: root.effectiveState === "happy"
    property bool isConfused: root.effectiveState === "confused"
    property bool isSad: root.effectiveState === "sad"
    property bool isAngry: root.effectiveState === "angry"
    property bool isSleeping: root.effectiveState === "sleeping"
    property color expressionAccent: {
        if (root.isAngry || root.isWarning) return Colors.warning
        if (root.isSad) return "#7AA7FF"
        if (root.isSpeaking) return Colors.info
        if (root.isHappy) return Colors.success
        if (root.styleLoona) return "#8AD8FF"
        if (root.styleOrb) return "#7EE7D8"
        return Colors.accent
    }

    function refreshSleepTimer() {
        if (!root.micActive && !root.speakingActive
                && (root.faceState === "idle" || root.faceState === "happy" || root.faceState === "sad")) {
            sleepTimer.restart()
        } else {
            sleepTimer.stop()
            if (root.sleepMode) {
                root.sleepMode = false
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.styleLoona ? "#101724" : "#151A21" }
            GradientStop { position: 1.0; color: root.styleLoona ? "#0B111A" : Colors.windowBg }
        }
    }

    Rectangle {
        id: glow
        width: root.styleOrb ? 430 : 380
        height: root.styleOrb ? 430 : 380
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
        width: root.styleLoona ? 470 : (root.styleOrb ? 360 : 520)
        height: root.styleLoona ? 270 : (root.styleOrb ? 360 : 250)
        radius: root.styleOrb ? 180 : (root.styleLoona ? 46 : 28)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        color: root.styleLoona ? "#0A121A" : Colors.panelBg
        border.width: 1
        border.color: root.expressionAccent
        y: (root.isSpeaking ? (-6 + root.talkBeat) : (root.isListening ? -2 : 0)) + root.idleBob + (root.isSleeping ? 4 : 0)
        scale: root.isSpeaking ? (1.014 + root.talkBeat * 0.004) : 1.0
        rotation: root.isConfused ? -1.8 : (root.isHappy ? 0.8 : (root.isAngry ? -0.5 : 0))

        Behavior on y { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 180 } }

        Column {
            anchors.centerIn: parent
            spacing: 26

            Row {
                spacing: root.styleLoona ? 86 : 106
                anchors.horizontalCenter: parent.horizontalCenter

                Rectangle {
                    width: 78
                    height: 7
                    radius: 4
                    color: root.expressionAccent
                    opacity: 0.9
                    rotation: root.isConfused ? -22 : ((root.isWarning || root.isAngry) ? -24 : (root.isHappy ? -16 : (root.isSad ? -4 : (-3 + root.moodSwing * 6 + root.talkBeat * 2.5))))
                    y: root.isThinking ? -4 : (root.isSad ? 3 : 0)
                    Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                }

                Rectangle {
                    width: 78
                    height: 7
                    radius: 4
                    color: root.expressionAccent
                    opacity: 0.9
                    rotation: root.isConfused ? 10 : ((root.isWarning || root.isAngry) ? 24 : (root.isHappy ? 16 : (root.isSad ? 4 : (3 - root.moodSwing * 6 - root.talkBeat * 2.5))))
                    y: root.isThinking ? -6 : (root.isSad ? 3 : 0)
                    Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                }
            }

            Row {
                spacing: root.styleLoona ? 52 : 72
                anchors.horizontalCenter: parent.horizontalCenter

                Eye {
                    openness: (root.isSleeping ? 0.18 : ((root.isWarning || root.isAngry) ? 0.42 : (root.isThinking ? 0.64 : (root.isHappy ? 0.9 : (root.isSad ? 0.72 : 0.94))))) * root.blinkFactor
                    focusX: root.isThinking ? -0.34 : (root.isListening ? 0.22 : (root.driftX + root.moodSwing * 0.14))
                    focusY: root.isThinking ? -0.2 : (root.isSpeaking ? (0.07 + root.talkBeat * 0.11) : (root.isHappy ? -0.04 : (root.isSad ? 0.04 : 0.0)))
                    squint: (root.isWarning || root.isAngry) ? 0.62 : (root.isSpeaking ? 0.2 : (root.isHappy ? 0.08 : 0.0))
                    pupilScale: root.isWarning ? 0.86 : (root.isSpeaking ? 1.08 : 1.0)
                    glintOpacity: (root.isWarning || root.isAngry) ? 0.35 : (root.isHappy ? 0.85 : (root.isSleeping ? 0.25 : 0.62))
                    warning: root.isWarning || root.isAngry
                    accentColor: root.expressionAccent
                    scale: root.styleLoona ? 1.18 : (root.styleOrb ? 1.06 : 1.0)
                }

                Eye {
                    openness: (root.isSleeping ? 0.18 : ((root.isWarning || root.isAngry) ? 0.42 : (root.isThinking ? 0.64 : (root.isHappy ? 0.9 : (root.isSad ? 0.72 : 0.94))))) * root.blinkFactor
                    focusX: root.isThinking ? 0.34 : (root.isListening ? -0.22 : (-root.driftX + root.moodSwing * 0.14))
                    focusY: root.isThinking ? -0.2 : (root.isSpeaking ? (0.07 + root.talkBeat * 0.11) : (root.isHappy ? -0.04 : (root.isSad ? 0.04 : 0.0)))
                    squint: (root.isWarning || root.isAngry) ? 0.62 : (root.isSpeaking ? 0.2 : (root.isHappy ? 0.08 : 0.0))
                    pupilScale: root.isWarning ? 0.86 : (root.isSpeaking ? 1.08 : 1.0)
                    glintOpacity: (root.isWarning || root.isAngry) ? 0.35 : (root.isHappy ? 0.85 : (root.isSleeping ? 0.25 : 0.62))
                    warning: root.isWarning || root.isAngry
                    accentColor: root.expressionAccent
                    scale: root.styleLoona ? 1.18 : (root.styleOrb ? 1.06 : 1.0)
                }
            }

            Mouth {
                anchors.horizontalCenter: parent.horizontalCenter
                stateName: root.effectiveState
                styleName: root.faceStyle
                beatMs: root.mouthBeatMs
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

    Rectangle {
        id: listenMeter
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: facePlate.bottom
        anchors.topMargin: 40
        width: 240
        height: 10
        radius: 5
        color: "#111922"
        border.width: 1
        border.color: "#2A3945"
        opacity: root.micActive ? 0.95 : 0.0
        visible: opacity > 0.01

        Behavior on opacity { NumberAnimation { duration: 150 } }

        Rectangle {
            width: Math.max(4, parent.width * root.voiceInputLevel)
            height: parent.height - 2
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.verticalCenter: parent.verticalCenter
            radius: 4
            color: root.voiceInputLevel > 0.62 ? Colors.warning : Colors.info
            opacity: 0.92
            Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    Text {
        anchors.right: facePlate.right
        anchors.rightMargin: 18
        anchors.top: facePlate.top
        anchors.topMargin: 16
        text: root.isSleeping ? "zZ" : ""
        color: Colors.textSecondary
        font.family: Typography.monoFamily
        font.pixelSize: 18
        opacity: root.isSleeping ? 0.9 : 0.0
        Behavior on opacity { NumberAnimation { duration: 220 } }
    }

    Timer {
        id: sleepTimer
        interval: 16000
        repeat: false
        onTriggered: {
            if (!root.micActive && !root.speakingActive && root.faceState === "idle") {
                root.sleepMode = true
            }
        }
    }

    onFaceStateChanged: {
        if (sleepMode && faceState !== "idle") {
            sleepMode = false
        }
        refreshSleepTimer()
    }

    onMicActiveChanged: refreshSleepTimer()
    onSpeakingActiveChanged: refreshSleepTimer()
    Component.onCompleted: refreshSleepTimer()

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
