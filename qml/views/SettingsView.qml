import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    property string provider: "Custom"
    property var providers: ["Custom", "LM Studio", "Ollama"]
    property var connectionProfiles: ["Default"]
    property string activeProfile: "Default"
    property string endpoint: ""
    property string modelsEndpoint: ""
    property string apiKey: ""
    property bool cameraEnabled: false
    property bool cameraAnalyzeRunning: false
    property var availableModels: []
    property string selectedModel: "local-prototype"
    property string modelStatus: ""
    property string latencySummary: "Latency n/a"
    property bool fastResponseMode: true
    property string smoothingProfile: "Balanced"
    property var smoothingProfiles: ["Instant", "Terminal", "Balanced", "Human", "Cinematic"]
    property int tokenRate: 100
    property int lipSyncDelayMs: 360
    property string assistantName: "Senticli"
    property bool wakeEnabled: true
    property var wakeResponses: ["How can I help you?"]
    property bool conversationalMode: true
    property bool duplexVoiceEnabled: false
    property string transcriptionEndpoint: ""
    property string transcriptionModel: "whisper-1"
    property int vadSensitivity: 50
    property string duplexSmoothness: "Balanced"
    property var duplexSmoothnessOptions: ["Responsive", "Balanced", "Natural", "Studio"]
    property string personality: "Helpful"
    property var personalities: ["Helpful", "Professional", "Witty", "Teacher", "Hacker", "Calm"]
    property string faceStyle: "Loona"
    property var faceStyles: ["Loona", "Terminal", "Orb", "Nova", "Pixel"]
    property string expressionIntensity: "Normal"
    property var expressionIntensityOptions: ["Subtle", "Normal", "Dramatic"]
    property string gender: "Neutral"
    property var genders: ["Neutral", "Male", "Female"]
    property string voiceStyle: "Default"
    property var voiceStyles: ["Default", "Soft", "Bright", "Narrator", "Human Female", "Human Male", "Studio"]
    property string voiceEngine: "Auto"
    property var voiceEngines: ["Auto", "Speech Dispatcher", "eSpeak", "Piper"]
    property string piperModelPath: ""
    property var availableVoiceModels: []
    property string selectedVoiceModel: ""
    property bool ttsEnabled: false
    property bool memoryEnabled: true
    property var grantedFolders: []
    property var auditEntries: []
    property bool setupComplete: false
    property bool streamingActive: false

    signal providerSelected(string provider)
    signal activeProfileSelected(string profileName)
    signal saveProfileRequested(string profileName)
    signal deleteProfileRequested(string profileName)
    signal endpointSubmitted(string endpoint)
    signal modelsEndpointSubmitted(string modelsEndpoint)
    signal apiKeySubmitted(string apiKey)
    signal cameraEnabledToggled(bool enabled)
    signal analyzeCameraRequested()
    signal refreshRequested()
    signal testRequested()
    signal modelSelected(string modelName)
    signal smoothingSelected(string profile)
    signal fastResponseModeToggled(bool enabled)
    signal tokenRateSelected(int rate)
    signal lipSyncDelaySelected(int delayMs)
    signal assistantNameSubmitted(string name)
    signal wakeEnabledToggled(bool enabled)
    signal wakeResponsesSubmitted(var responses)
    signal conversationalModeToggled(bool enabled)
    signal duplexVoiceEnabledToggled(bool enabled)
    signal transcriptionEndpointSubmitted(string endpoint)
    signal transcriptionModelSubmitted(string model)
    signal vadSensitivitySelected(int value)
    signal duplexSmoothnessSelected(string value)
    signal personalitySelected(string personality)
    signal faceStyleSelected(string faceStyle)
    signal expressionIntensitySelected(string value)
    signal genderSelected(string gender)
    signal voiceStyleSelected(string voiceStyle)
    signal voiceEngineSelected(string voiceEngine)
    signal piperModelPathSubmitted(string path)
    signal refreshVoiceModelsRequested()
    signal selectedVoiceModelSubmitted(string path)
    signal voiceTestRequested()
    signal ttsToggledRequested(bool enabled)
    signal memoryToggledRequested(bool enabled)
    signal addFolderRequested(string folder)
    signal removeFolderRequested(string folder)
    signal clearAuditRequested()
    signal completeSetupRequested()
    signal closeRequested()

    color: Colors.panelBg
    radius: 14
    border.width: 1
    border.color: Colors.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "AI Settings"
                color: Colors.textPrimary
                font.family: Typography.uiFamily
                font.pixelSize: Typography.titleSize
            }

            Item { Layout.fillWidth: true }

            Button {
                visible: !root.setupComplete
                text: "Complete Setup"
                highlighted: true
                onClicked: root.completeSetupRequested()
                font.family: Typography.uiFamily
                font.pixelSize: Typography.smallSize
            }

            Button {
                text: "Close"
                onClicked: root.closeRequested()
                font.family: Typography.uiFamily
                font.pixelSize: Typography.smallSize
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                width: root.width - 28
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    color: Colors.panelElevated
                    radius: 10
                    border.width: 1
                    border.color: Colors.border
                    implicitHeight: providerBlock.implicitHeight + 16

                    ColumnLayout {
                        id: providerBlock
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: "Profile + Provider + Endpoint"
                            color: Colors.textSecondary
                            font.family: Typography.uiFamily
                            font.pixelSize: Typography.smallSize
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            ComboBox {
                                id: profileCombo
                                Layout.preferredWidth: 220
                                model: root.connectionProfiles
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.activeProfileSelected(currentText)
                            }

                            TextField {
                                id: profileNameField
                                Layout.fillWidth: true
                                placeholderText: "Profile name"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: {
                                    const value = text.trim()
                                    if (value.length > 0) {
                                        root.saveProfileRequested(value)
                                    }
                                }
                            }

                            Button {
                                text: "Save Profile"
                                onClicked: {
                                    const value = profileNameField.text.trim()
                                    if (value.length > 0) {
                                        root.saveProfileRequested(value)
                                        return
                                    }
                                    if (profileCombo.currentText.length > 0) {
                                        root.saveProfileRequested(profileCombo.currentText)
                                    }
                                }
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Delete"
                                onClicked: {
                                    const value = profileNameField.text.trim().length > 0
                                                    ? profileNameField.text.trim()
                                                    : profileCombo.currentText
                                    if (value.length > 0) {
                                        root.deleteProfileRequested(value)
                                    }
                                }
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            ComboBox {
                                id: providerCombo
                                Layout.preferredWidth: 180
                                model: root.providers
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.providerSelected(currentText)
                            }

                            TextField {
                                id: endpointField
                                Layout.fillWidth: true
                                placeholderText: "192.168.1.220/v1/chat/completions"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: root.endpointSubmitted(text)
                            }

                            Button {
                                text: "Set"
                                onClicked: root.endpointSubmitted(endpointField.text)
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Test"
                                enabled: !root.streamingActive
                                onClicked: root.testRequested()
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            TextField {
                                id: modelsEndpointField
                                Layout.fillWidth: true
                                placeholderText: "Optional models endpoint override (auto uses /v1/models)"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: root.modelsEndpointSubmitted(text)
                            }

                            Button {
                                text: "Save Models URL"
                                onClicked: root.modelsEndpointSubmitted(modelsEndpointField.text)
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Auto"
                                onClicked: {
                                    modelsEndpointField.text = ""
                                    root.modelsEndpointSubmitted("")
                                }
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            TextField {
                                id: apiKeyField
                                Layout.fillWidth: true
                                placeholderText: "Optional API key / bearer token"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                echoMode: TextInput.Password
                                onAccepted: root.apiKeySubmitted(text)
                            }

                            Button {
                                text: "Save Key"
                                onClicked: root.apiKeySubmitted(apiKeyField.text)
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Clear"
                                onClicked: {
                                    apiKeyField.text = ""
                                    root.apiKeySubmitted("")
                                }
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Switch {
                                id: cameraEnabledSwitch
                                text: "Camera Access"
                                checked: root.cameraEnabled
                                onToggled: root.cameraEnabledToggled(checked)
                            }

                            Button {
                                text: "Analyze Current Frame"
                                enabled: root.cameraEnabled && !root.cameraAnalyzeRunning && !root.streamingActive
                                onClicked: root.analyzeCameraRequested()
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Label {
                                text: root.cameraAnalyzeRunning ? "Analyzing..." : ""
                                color: Colors.textSecondary
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    color: Colors.panelElevated
                    radius: 10
                    border.width: 1
                    border.color: Colors.border
                    implicitHeight: identityBlock.implicitHeight + 16

                    ColumnLayout {
                        id: identityBlock
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: "Companion Identity"
                            color: Colors.textSecondary
                            font.family: Typography.uiFamily
                            font.pixelSize: Typography.smallSize
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            TextField {
                                id: assistantNameField
                                Layout.preferredWidth: 220
                                text: root.assistantName
                                placeholderText: "Assistant name"
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: root.assistantNameSubmitted(text)
                            }

                            Button {
                                text: "Set Name"
                                onClicked: root.assistantNameSubmitted(assistantNameField.text)
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Switch {
                                id: wakeEnabledSwitch
                                text: "Wake Phrase"
                                checked: root.wakeEnabled
                                onToggled: root.wakeEnabledToggled(checked)
                            }

                            Switch {
                                id: conversationalSwitch
                                text: "Conversational Replies"
                                checked: root.conversationalMode
                                onToggled: root.conversationalModeToggled(checked)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Switch {
                                id: duplexVoiceSwitch
                                text: "Duplex Voice (Beta)"
                                checked: root.duplexVoiceEnabled
                                onToggled: root.duplexVoiceEnabledToggled(checked)
                            }

                            TextField {
                                id: sttEndpointField
                                Layout.fillWidth: true
                                placeholderText: "STT endpoint (auto: /v1/audio/transcriptions)"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: root.transcriptionEndpointSubmitted(text)
                            }

                            Button {
                                text: "Set STT URL"
                                onClicked: root.transcriptionEndpointSubmitted(sttEndpointField.text)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            TextField {
                                id: sttModelField
                                Layout.preferredWidth: 220
                                placeholderText: "STT model (example: whisper-1)"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: root.transcriptionModelSubmitted(text)
                            }

                            Button {
                                text: "Set STT Model"
                                onClicked: root.transcriptionModelSubmitted(sttModelField.text)
                            }

                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Duplex Smoothness"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            ComboBox {
                                id: duplexSmoothnessCombo
                                Layout.preferredWidth: 180
                                model: root.duplexSmoothnessOptions
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.duplexSmoothnessSelected(currentText)
                            }

                            Label {
                                text: "VAD Sensitivity"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Slider {
                                id: vadSensitivitySlider
                                Layout.fillWidth: true
                                from: 1
                                to: 100
                                stepSize: 1
                                live: false
                                onMoved: root.vadSensitivitySelected(Math.round(value))
                                onPressedChanged: {
                                    if (!pressed) {
                                        root.vadSensitivitySelected(Math.round(value))
                                    }
                                }
                            }

                            Label {
                                text: Math.round(vadSensitivitySlider.value).toString()
                                color: Colors.textPrimary
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Quick Tune"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Fast"
                                onClicked: {
                                    root.duplexSmoothnessSelected("Responsive")
                                    root.vadSensitivitySelected(58)
                                }
                            }

                            Button {
                                text: "Balanced"
                                onClicked: {
                                    root.duplexSmoothnessSelected("Balanced")
                                    root.vadSensitivitySelected(50)
                                }
                            }

                            Button {
                                text: "Human"
                                highlighted: true
                                onClicked: {
                                    root.duplexSmoothnessSelected("Natural")
                                    root.vadSensitivitySelected(64)
                                }
                            }

                            Button {
                                text: "Studio"
                                onClicked: {
                                    root.duplexSmoothnessSelected("Studio")
                                    root.vadSensitivitySelected(42)
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Label {
                            text: "Wake responses (one line each)"
                            color: Colors.textSecondary
                            font.family: Typography.uiFamily
                            font.pixelSize: Typography.smallSize
                        }

                        TextArea {
                            id: wakeResponsesArea
                            Layout.fillWidth: true
                            Layout.preferredHeight: 96
                            wrapMode: TextArea.Wrap
                            font.family: Typography.monoFamily
                            font.pixelSize: Typography.smallSize
                            placeholderText: "How can I help you?\nReady when you are."
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "Save Wake Responses"
                                onClicked: {
                                    const lines = wakeResponsesArea.text.split("\n")
                                    root.wakeResponsesSubmitted(lines)
                                }
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    color: Colors.panelElevated
                    radius: 10
                    border.width: 1
                    border.color: Colors.border
                    implicitHeight: modelBlock.implicitHeight + 16

                    ColumnLayout {
                        id: modelBlock
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: "Model"
                            color: Colors.textSecondary
                            font.family: Typography.uiFamily
                            font.pixelSize: Typography.smallSize
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            ComboBox {
                                id: modelCombo
                                Layout.fillWidth: true
                                model: root.availableModels
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.modelSelected(currentText)
                            }

                            Button {
                                text: "Refresh"
                                enabled: !root.streamingActive
                                onClicked: root.refreshRequested()
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Use"
                                onClicked: {
                                    if (manualModelField.text.trim().length > 0) {
                                        root.modelSelected(manualModelField.text.trim())
                                    } else if (modelCombo.currentText.length > 0) {
                                        root.modelSelected(modelCombo.currentText)
                                    }
                                }
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            TextField {
                                id: manualModelField
                                Layout.fillWidth: true
                                placeholderText: "Type model id manually (example: qwen2.5-coder:14b)"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: {
                                    const value = text.trim()
                                    if (value.length > 0) {
                                        root.modelSelected(value)
                                        clear()
                                    }
                                }
                            }

                            Button {
                                text: "Set Typed Model"
                                onClicked: {
                                    const value = manualModelField.text.trim()
                                    if (value.length > 0) {
                                        root.modelSelected(value)
                                        manualModelField.clear()
                                    }
                                }
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    color: Colors.panelElevated
                    radius: 10
                    border.width: 1
                    border.color: Colors.border
                    implicitHeight: behaviorBlock.implicitHeight + 16

                    ColumnLayout {
                        id: behaviorBlock
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: "Streaming + Behavior"
                            color: Colors.textSecondary
                            font.family: Typography.uiFamily
                            font.pixelSize: Typography.smallSize
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            ComboBox {
                                id: smoothingCombo
                                Layout.preferredWidth: 220
                                model: root.smoothingProfiles
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.smoothingSelected(currentText)
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "Instant: raw speed, Terminal: fast, Balanced: default, Human/Cinematic: smoother"
                                color: Colors.textSecondary
                                elide: Label.ElideRight
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Switch {
                                id: fastResponseSwitch
                                text: "Fast Response"
                                checked: root.fastResponseMode
                                onToggled: root.fastResponseModeToggled(checked)
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.latencySummary
                                color: Colors.textSecondary
                                elide: Label.ElideRight
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Persona"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            ComboBox {
                                id: personalityCombo
                                Layout.preferredWidth: 180
                                model: root.personalities
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.personalitySelected(currentText)
                            }

                            Label {
                                text: "Face Style"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            ComboBox {
                                id: faceStyleCombo
                                Layout.preferredWidth: 150
                                model: root.faceStyles
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.faceStyleSelected(currentText)
                            }

                            Label {
                                text: "Expression"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            ComboBox {
                                id: expressionIntensityCombo
                                Layout.preferredWidth: 140
                                model: root.expressionIntensityOptions
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.expressionIntensitySelected(currentText)
                            }

                            Label {
                                text: "Voice Gender"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            ComboBox {
                                id: genderCombo
                                Layout.preferredWidth: 140
                                model: root.genders
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.genderSelected(currentText)
                            }

                            Label {
                                text: "Voice Style"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            ComboBox {
                                id: voiceStyleCombo
                                Layout.preferredWidth: 160
                                model: root.voiceStyles
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.voiceStyleSelected(currentText)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Voice Engine"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            ComboBox {
                                id: voiceEngineCombo
                                Layout.preferredWidth: 190
                                model: root.voiceEngines
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                                onActivated: root.voiceEngineSelected(currentText)
                            }

                            TextField {
                                id: piperModelField
                                Layout.fillWidth: true
                                placeholderText: "Piper model path (.onnx) or leave blank"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: root.piperModelPathSubmitted(text)
                            }

                            Button {
                                text: "Set Piper Model"
                                onClicked: root.piperModelPathSubmitted(piperModelField.text)
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Test Voice"
                                enabled: !root.streamingActive
                                onClicked: root.voiceTestRequested()
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Real Voice Model"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            ComboBox {
                                id: voiceModelCombo
                                Layout.fillWidth: true
                                model: root.availableVoiceModels
                                editable: false
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Use Selected"
                                enabled: voiceModelCombo.currentText.length > 0
                                onClicked: {
                                    if (voiceModelCombo.currentText.length > 0) {
                                        root.selectedVoiceModelSubmitted(voiceModelCombo.currentText)
                                    }
                                }
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Button {
                                text: "Refresh Voices"
                                onClicked: root.refreshVoiceModelsRequested()
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Token Rate"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Slider {
                                id: tokenRateSlider
                                Layout.fillWidth: true
                                from: 20
                                to: 600
                                stepSize: 5
                                live: false
                                onMoved: root.tokenRateSelected(Math.round(value))
                                onValueChanged: {
                                    if (!pressed) {
                                        root.tokenRateSelected(Math.round(value))
                                    }
                                }
                            }

                            Label {
                                text: Math.round(tokenRateSlider.value) + "%"
                                color: Colors.textPrimary
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Lip Sync Delay"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Slider {
                                id: lipSyncSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 1100
                                stepSize: 10
                                live: false
                                onMoved: root.lipSyncDelaySelected(Math.round(value))
                                onValueChanged: {
                                    if (!pressed) {
                                        root.lipSyncDelaySelected(Math.round(value))
                                    }
                                }
                            }

                            Label {
                                text: Math.round(lipSyncSlider.value) + " ms"
                                color: Colors.textPrimary
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 16

                            Switch {
                                id: ttsSwitch
                                text: "Voice Output"
                                checked: root.ttsEnabled
                                onToggled: root.ttsToggledRequested(checked)
                            }

                            Switch {
                                id: memorySwitch
                                text: "Project Memory"
                                checked: root.memoryEnabled
                                onToggled: root.memoryToggledRequested(checked)
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    color: Colors.panelElevated
                    radius: 10
                    border.width: 1
                    border.color: Colors.border
                    implicitHeight: permissionsBlock.implicitHeight + 16

                    ColumnLayout {
                        id: permissionsBlock
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: "Permissions"
                            color: Colors.textSecondary
                            font.family: Typography.uiFamily
                            font.pixelSize: Typography.smallSize
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            TextField {
                                id: folderField
                                Layout.fillWidth: true
                                placeholderText: "~/Documents or /home/user/Projects"
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                onAccepted: {
                                    root.addFolderRequested(text)
                                    clear()
                                }
                            }

                            Button {
                                text: "Grant"
                                onClicked: {
                                    root.addFolderRequested(folderField.text)
                                    folderField.clear()
                                }
                            }
                        }

                        Repeater {
                            model: root.grantedFolders
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData
                                    color: Colors.textPrimary
                                    font.family: Typography.monoFamily
                                    font.pixelSize: Typography.smallSize
                                    elide: Label.ElideMiddle
                                }
                                Button {
                                    text: "Remove"
                                    onClicked: root.removeFolderRequested(modelData)
                                }
                            }
                        }

                        Label {
                            visible: root.grantedFolders.length === 0
                            text: "No folders granted yet."
                            color: Colors.textSecondary
                            font.family: Typography.monoFamily
                            font.pixelSize: Typography.smallSize
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    color: Colors.panelElevated
                    radius: 10
                    border.width: 1
                    border.color: Colors.border
                    implicitHeight: auditBlock.implicitHeight + 16

                    ColumnLayout {
                        id: auditBlock
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Audit Log"
                                color: Colors.textSecondary
                                font.family: Typography.uiFamily
                                font.pixelSize: Typography.smallSize
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                text: "Clear"
                                onClicked: root.clearAuditRequested()
                            }
                        }

                        ListView {
                            id: auditList
                            Layout.fillWidth: true
                            Layout.preferredHeight: 140
                            model: root.auditEntries
                            clip: true
                            spacing: 4
                            delegate: Text {
                                text: modelData
                                wrapMode: Text.Wrap
                                color: Colors.textPrimary
                                font.family: Typography.monoFamily
                                font.pixelSize: Typography.smallSize
                                width: auditList.width
                            }
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.modelStatus
            color: Colors.textSecondary
            elide: Label.ElideRight
            font.family: Typography.monoFamily
            font.pixelSize: Typography.smallSize
        }
    }

    onProviderChanged: {
        const idx = providerCombo.find(provider)
        if (idx >= 0) {
            providerCombo.currentIndex = idx
        }
    }

    onActiveProfileChanged: {
        const idx = profileCombo.find(activeProfile)
        if (idx >= 0) {
            profileCombo.currentIndex = idx
        }
    }

    onConnectionProfilesChanged: {
        const idx = profileCombo.find(activeProfile)
        if (idx >= 0) {
            profileCombo.currentIndex = idx
        }
    }

    onEndpointChanged: {
        endpointField.text = endpoint
    }

    onModelsEndpointChanged: {
        modelsEndpointField.text = modelsEndpoint
    }

    onApiKeyChanged: {
        apiKeyField.text = apiKey
    }

    onCameraEnabledChanged: {
        cameraEnabledSwitch.checked = cameraEnabled
    }

    onSelectedModelChanged: {
        const idx = modelCombo.find(selectedModel)
        if (idx >= 0) {
            modelCombo.currentIndex = idx
        }
    }

    onSmoothingProfileChanged: {
        const idx = smoothingCombo.find(smoothingProfile)
        if (idx >= 0) {
            smoothingCombo.currentIndex = idx
        }
    }

    onFastResponseModeChanged: {
        fastResponseSwitch.checked = fastResponseMode
    }

    onPersonalityChanged: {
        const idx = personalityCombo.find(personality)
        if (idx >= 0) {
            personalityCombo.currentIndex = idx
        }
    }

    onFaceStyleChanged: {
        const idx = faceStyleCombo.find(faceStyle)
        if (idx >= 0) {
            faceStyleCombo.currentIndex = idx
        }
    }

    onExpressionIntensityChanged: {
        const idx = expressionIntensityCombo.find(expressionIntensity)
        if (idx >= 0) {
            expressionIntensityCombo.currentIndex = idx
        }
    }

    onGenderChanged: {
        const idx = genderCombo.find(gender)
        if (idx >= 0) {
            genderCombo.currentIndex = idx
        }
    }

    onVoiceStyleChanged: {
        const idx = voiceStyleCombo.find(voiceStyle)
        if (idx >= 0) {
            voiceStyleCombo.currentIndex = idx
        }
    }

    onVoiceEngineChanged: {
        const idx = voiceEngineCombo.find(voiceEngine)
        if (idx >= 0) {
            voiceEngineCombo.currentIndex = idx
        }
    }

    onAvailableVoiceModelsChanged: {
        const idx = voiceModelCombo.find(selectedVoiceModel)
        if (idx >= 0) {
            voiceModelCombo.currentIndex = idx
        }
    }

    onSelectedVoiceModelChanged: {
        const idx = voiceModelCombo.find(selectedVoiceModel)
        if (idx >= 0) {
            voiceModelCombo.currentIndex = idx
        }
    }

    onPiperModelPathChanged: {
        piperModelField.text = piperModelPath
    }

    onTokenRateChanged: {
        tokenRateSlider.value = tokenRate
    }

    onLipSyncDelayMsChanged: {
        lipSyncSlider.value = lipSyncDelayMs
    }

    onAssistantNameChanged: {
        assistantNameField.text = assistantName
    }

    onWakeEnabledChanged: {
        wakeEnabledSwitch.checked = wakeEnabled
    }

    onWakeResponsesChanged: {
        wakeResponsesArea.text = wakeResponses.join("\n")
    }

    onConversationalModeChanged: {
        conversationalSwitch.checked = conversationalMode
    }

    onDuplexVoiceEnabledChanged: {
        duplexVoiceSwitch.checked = duplexVoiceEnabled
    }

    onTranscriptionEndpointChanged: {
        sttEndpointField.text = transcriptionEndpoint
    }

    onTranscriptionModelChanged: {
        sttModelField.text = transcriptionModel
    }

    onVadSensitivityChanged: {
        vadSensitivitySlider.value = vadSensitivity
    }

    onDuplexSmoothnessChanged: {
        const idx = duplexSmoothnessCombo.find(duplexSmoothness)
        if (idx >= 0) {
            duplexSmoothnessCombo.currentIndex = idx
        }
    }

    onTtsEnabledChanged: {
        ttsSwitch.checked = ttsEnabled
    }

    onMemoryEnabledChanged: {
        memorySwitch.checked = memoryEnabled
    }

    Component.onCompleted: {
        endpointField.text = endpoint
        modelsEndpointField.text = modelsEndpoint
        apiKeyField.text = apiKey
        cameraEnabledSwitch.checked = cameraEnabled
        tokenRateSlider.value = tokenRate
        lipSyncSlider.value = lipSyncDelayMs
        assistantNameField.text = assistantName
        wakeEnabledSwitch.checked = wakeEnabled
        wakeResponsesArea.text = wakeResponses.join("\n")
        conversationalSwitch.checked = conversationalMode
        duplexVoiceSwitch.checked = duplexVoiceEnabled
        sttEndpointField.text = transcriptionEndpoint
        sttModelField.text = transcriptionModel
        vadSensitivitySlider.value = vadSensitivity
        ttsSwitch.checked = ttsEnabled
        memorySwitch.checked = memoryEnabled
        piperModelField.text = piperModelPath
        fastResponseSwitch.checked = fastResponseMode

        let idx = profileCombo.find(activeProfile)
        if (idx >= 0) {
            profileCombo.currentIndex = idx
        }

        idx = providerCombo.find(provider)
        if (idx >= 0) {
            providerCombo.currentIndex = idx
        }

        idx = modelCombo.find(selectedModel)
        if (idx >= 0) {
            modelCombo.currentIndex = idx
        }

        idx = smoothingCombo.find(smoothingProfile)
        if (idx >= 0) {
            smoothingCombo.currentIndex = idx
        }

        idx = personalityCombo.find(personality)
        if (idx >= 0) {
            personalityCombo.currentIndex = idx
        }

        idx = faceStyleCombo.find(faceStyle)
        if (idx >= 0) {
            faceStyleCombo.currentIndex = idx
        }

        idx = expressionIntensityCombo.find(expressionIntensity)
        if (idx >= 0) {
            expressionIntensityCombo.currentIndex = idx
        }

        idx = genderCombo.find(gender)
        if (idx >= 0) {
            genderCombo.currentIndex = idx
        }

        idx = voiceStyleCombo.find(voiceStyle)
        if (idx >= 0) {
            voiceStyleCombo.currentIndex = idx
        }

        idx = voiceEngineCombo.find(voiceEngine)
        if (idx >= 0) {
            voiceEngineCombo.currentIndex = idx
        }

        idx = voiceModelCombo.find(selectedVoiceModel)
        if (idx >= 0) {
            voiceModelCombo.currentIndex = idx
        }

        idx = duplexSmoothnessCombo.find(duplexSmoothness)
        if (idx >= 0) {
            duplexSmoothnessCombo.currentIndex = idx
        }
    }
}
