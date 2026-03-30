import QtQuick
import QtQuick.Layouts
import Senticli

Item {
    id: root
    property string faceState: "idle"
    property string statusText: "Ready"
    property bool micActive: false
    property real voiceInputLevel: 0.0
    property bool cameraEnabled: false
    property bool cameraAnalyzeRunning: false
    property string faceStyle: "Loona"
    property int mouthBeatMs: 180
    property bool speakingActive: false

    property real blinkFactor: 1.0
    property real driftX: 0.0
    property real driftY: 0.0
    property real moodSwing: 0.0
    property real idleBob: 0.0
    property real talkBeat: 0.0
    property real shakeOffset: 0.0
    property real listenPulse: 0.0
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
    readonly property bool expressionActive: root.isListening || root.isSpeaking || root.isThinking
                                            || root.isHappy || root.isConfused || root.isSad
                                            || root.isAngry || root.isWarning
    readonly property bool showKissEmote: !root.isSpeaking && root.isListening && root.voiceInputLevel > 0.56
    readonly property string mouthEmote: {
        if (root.showKissEmote) return "kiss"
        if (root.isHappy) return "smileBig"
        if (root.isSad) return "frown"
        if (root.isConfused) return "wavy"
        if (root.isAngry || root.isWarning) return "grimace"
        return "none"
    }
    readonly property bool showMouthEmote: root.mouthEmote !== "none"
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

    function captureAndAnalyzeFrame() {
        if (!cameraEnabled || cameraAnalyzeRunning) {
            return
        }
        if (typeof backend !== "undefined" && backend.captureAndAnalyzeCameraFrame) {
            backend.captureAndAnalyzeCameraFrame()
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
            running: root.expressionActive && !root.isSleeping
            NumberAnimation { from: 0.985; to: 1.024; duration: root.isSpeaking ? 640 : (root.isListening ? 820 : 1220); easing.type: Easing.InOutQuad }
            NumberAnimation { from: 1.024; to: 0.985; duration: root.isSpeaking ? 640 : (root.isListening ? 820 : 1220); easing.type: Easing.InOutQuad }
        }
    }

    Rectangle {
        id: cameraPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 10
        anchors.rightMargin: 10
        width: 252
        height: 154
        radius: 10
        color: "#0A1016"
        border.width: 1
        border.color: cameraEnabled ? root.expressionAccent : "#2C3640"
        visible: true
        opacity: cameraEnabled ? 0.96 : 0.55

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 8
            color: "#111720"

            Column {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.cameraEnabled ? "Camera Ready" : "Camera Off"
                    color: Colors.textSecondary
                    font.family: Typography.monoFamily
                    font.pixelSize: Typography.smallSize
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.cameraEnabled ? "Click to capture + analyze" : "Enable camera in AI Settings"
                    color: "#6E8295"
                    font.family: Typography.monoFamily
                    font.pixelSize: 11
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 8
            anchors.bottomMargin: 8
            radius: 7
            color: "#111722"
            opacity: 0.9
            width: camLabel.implicitWidth + 14
            height: 22

            Text {
                id: camLabel
                anchors.centerIn: parent
                text: root.cameraAnalyzeRunning ? "Analyzing..." : "Camera"
                color: Colors.textSecondary
                font.family: Typography.monoFamily
                font.pixelSize: 11
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: root.cameraEnabled && !root.cameraAnalyzeRunning
            onClicked: root.captureAndAnalyzeFrame()
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
        x: root.isAngry || root.isWarning ? root.shakeOffset : 0
        y: (root.isSpeaking ? (-6 + root.talkBeat) : (root.isListening ? (-2 + root.listenPulse * 1.1) : 0))
           + root.idleBob + (root.isSleeping ? 4 : 0) + (root.driftY * 3.2)
        scale: root.isSpeaking
               ? (1.014 + root.talkBeat * 0.004)
               : (root.isHappy ? 1.008 + root.listenPulse * 0.002 : 1.0)
        rotation: root.isConfused
                  ? -1.8
                  : (root.isHappy ? (0.8 + root.listenPulse * 0.8) : (root.isAngry ? -0.5 : 0))

        Behavior on x { NumberAnimation { duration: 70; easing.type: Easing.InOutQuad } }
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
                    focusY: (root.isThinking ? -0.2 : (root.isSpeaking ? (0.07 + root.talkBeat * 0.11) : (root.isHappy ? -0.04 : (root.isSad ? 0.04 : 0.0))))
                            + (root.driftY * 0.28)
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
                    focusY: (root.isThinking ? -0.2 : (root.isSpeaking ? (0.07 + root.talkBeat * 0.11) : (root.isHappy ? -0.04 : (root.isSad ? 0.04 : 0.0))))
                            + (root.driftY * 0.28)
                    squint: (root.isWarning || root.isAngry) ? 0.62 : (root.isSpeaking ? 0.2 : (root.isHappy ? 0.08 : 0.0))
                    pupilScale: root.isWarning ? 0.86 : (root.isSpeaking ? 1.08 : 1.0)
                    glintOpacity: (root.isWarning || root.isAngry) ? 0.35 : (root.isHappy ? 0.85 : (root.isSleeping ? 0.25 : 0.62))
                    warning: root.isWarning || root.isAngry
                    accentColor: root.expressionAccent
                    scale: root.styleLoona ? 1.18 : (root.styleOrb ? 1.06 : 1.0)
                }
            }

            Item {
                id: emoteLayer
                width: 192
                height: 64
                anchors.horizontalCenter: parent.horizontalCenter
                visible: root.showMouthEmote
                opacity: root.showMouthEmote ? 1.0 : 0.0

                Behavior on opacity { NumberAnimation { duration: 110 } }

                Item {
                    anchors.centerIn: parent
                    width: 170
                    height: 58
                    clip: true
                    visible: root.mouthEmote === "smileBig"

                    Rectangle {
                        width: 170
                        height: 170
                        radius: 85
                        x: 0
                        y: -132
                        color: "transparent"
                        border.width: 6
                        border.color: "#6BEAB5"
                    }
                }

                Item {
                    anchors.centerIn: parent
                    width: 120
                    height: 52
                    visible: root.mouthEmote === "kiss"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 34 + (root.voiceInputLevel * 24)
                        height: 28 + (root.voiceInputLevel * 12)
                        radius: width / 2
                        color: "transparent"
                        border.width: 4
                        border.color: "#FFC6DE"
                        scale: 0.95 + (root.listenPulse * 0.05)
                    }

                    Rectangle {
                        anchors.left: parent.horizontalCenter
                        anchors.leftMargin: 24
                        anchors.verticalCenter: parent.verticalCenter
                        width: 10
                        height: 10
                        radius: 5
                        color: "#FFC6DE"
                        opacity: 0.85
                    }
                }

                Item {
                    anchors.centerIn: parent
                    width: 160
                    height: 52
                    clip: true
                    visible: root.mouthEmote === "frown"

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
                    width: 126 + (root.moodSwing * 4)
                    height: 12
                    radius: 6
                    color: root.isAngry ? "#FF7A56" : Colors.warning
                    opacity: 0.92
                    rotation: root.isAngry ? (2 + root.shakeOffset) : 0
                    visible: root.mouthEmote === "grimace"
                    Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                }

                Text {
                    anchors.centerIn: parent
                    visible: root.mouthEmote === "wavy"
                    text: "~  ~"
                    color: Colors.warning
                    font.family: Typography.monoFamily
                    font.pixelSize: 28
                    font.bold: true
                }
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
        interval: 2500
        running: true
        repeat: true
        onTriggered: {
            blink.restart()
            blinkTimer.interval = 1800 + Math.random() * 2000
        }
    }

    Timer {
        id: lookTimer
        interval: root.isListening ? 1150 : (root.isThinking ? 980 : 1800)
        running: true
        repeat: true
        onTriggered: {
            if (root.isThinking) {
                root.driftX = (Math.random() * 0.26) - 0.13
                root.driftY = -0.18 + Math.random() * 0.08
                return
            }
            if (root.isListening) {
                root.driftX = (Math.random() * 0.48) - 0.24
                root.driftY = (Math.random() * 0.16) - 0.08
                return
            }
            if (root.isWarning || root.isAngry) {
                root.driftX = (Math.random() * 0.14) - 0.07
                root.driftY = 0.03
                return
            }
            root.driftX = (Math.random() * 0.7) - 0.35
            root.driftY = (Math.random() * 0.14) - 0.07
        }
    }

    Timer {
        id: shakeTimer
        interval: 70
        running: root.isAngry || root.isWarning
        repeat: true
        onTriggered: {
            root.shakeOffset = (Math.random() * 3.6) - 1.8
        }
    }

    onIsAngryChanged: {
        if (!root.isAngry && !root.isWarning) {
            root.shakeOffset = 0
        }
    }
    onIsWarningChanged: {
        if (!root.isAngry && !root.isWarning) {
            root.shakeOffset = 0
        }
    }

    SequentialAnimation on moodSwing {
        loops: Animation.Infinite
        running: root.expressionActive && !root.isSleeping
        NumberAnimation { from: -1.0; to: 1.0; duration: root.isSpeaking ? 820 : 1700; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.0; to: -1.0; duration: root.isSpeaking ? 820 : 1700; easing.type: Easing.InOutSine }
    }

    SequentialAnimation on idleBob {
        loops: Animation.Infinite
        running: !root.isSpeaking && !root.isSleeping
        NumberAnimation { from: -1.0; to: 1.2; duration: 2100; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.2; to: -1.0; duration: 2100; easing.type: Easing.InOutSine }
    }

    SequentialAnimation on talkBeat {
        loops: Animation.Infinite
        running: root.isSpeaking
        NumberAnimation { from: -1.0; to: 1.0; duration: Math.max(140, Math.round(root.mouthBeatMs * 0.75)); easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.0; to: -1.0; duration: Math.max(140, Math.round(root.mouthBeatMs * 0.75)); easing.type: Easing.InOutSine }
    }

    SequentialAnimation on listenPulse {
        loops: Animation.Infinite
        running: root.isListening && !root.isSpeaking
        NumberAnimation { from: -1.0; to: 1.0; duration: 500; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.0; to: -1.0; duration: 500; easing.type: Easing.InOutSine }
    }

    SequentialAnimation {
        id: blink
        NumberAnimation { target: root; property: "blinkFactor"; to: 0.12; duration: 80; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root; property: "blinkFactor"; to: 1.0; duration: 110; easing.type: Easing.InOutQuad }
    }
}
