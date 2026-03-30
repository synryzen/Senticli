import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    property string provider: "Custom"
    property var providers: ["Custom", "LM Studio", "Ollama"]
    property string endpoint: ""
    property string apiKey: ""
    property var availableModels: []
    property string selectedModel: "local-prototype"
    property string modelStatus: ""
    property string smoothingProfile: "Balanced"
    property var smoothingProfiles: ["Cinematic", "Human", "Balanced", "Terminal"]
    property int tokenRate: 100
    property bool ttsEnabled: false
    property bool memoryEnabled: true
    property var grantedFolders: []
    property var auditEntries: []
    property bool setupComplete: false
    property bool streamingActive: false

    signal providerSelected(string provider)
    signal endpointSubmitted(string endpoint)
    signal apiKeySubmitted(string apiKey)
    signal refreshRequested()
    signal testRequested()
    signal modelSelected(string modelName)
    signal smoothingSelected(string profile)
    signal tokenRateSelected(int rate)
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
                            text: "Provider + Endpoint"
                            color: Colors.textSecondary
                            font.family: Typography.uiFamily
                            font.pixelSize: Typography.smallSize
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
                                text: "Cinematic: smoothest, Human: slow, Balanced: default, Terminal: fast"
                                color: Colors.textSecondary
                                elide: Label.ElideRight
                                font.family: Typography.monoFamily
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
                                to: 300
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

    onEndpointChanged: {
        endpointField.text = endpoint
    }

    onApiKeyChanged: {
        apiKeyField.text = apiKey
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

    onTokenRateChanged: {
        tokenRateSlider.value = tokenRate
    }

    onTtsEnabledChanged: {
        ttsSwitch.checked = ttsEnabled
    }

    onMemoryEnabledChanged: {
        memorySwitch.checked = memoryEnabled
    }

    Component.onCompleted: {
        endpointField.text = endpoint
        apiKeyField.text = apiKey
        tokenRateSlider.value = tokenRate
        ttsSwitch.checked = ttsEnabled
        memorySwitch.checked = memoryEnabled

        let idx = providerCombo.find(provider)
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
    }
}
