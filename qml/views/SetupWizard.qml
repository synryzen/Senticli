import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    property string provider: "Custom"
    property var providers: ["Custom", "LM Studio", "Ollama"]
    property string endpoint: ""
    property string modelsEndpoint: ""
    property string apiKey: ""
    property var availableModels: []
    property string selectedModel: "local-prototype"
    property bool ttsEnabled: false
    property bool memoryEnabled: true

    signal providerSelected(string provider)
    signal endpointSubmitted(string endpoint)
    signal modelsEndpointSubmitted(string modelsEndpoint)
    signal apiKeySubmitted(string apiKey)
    signal refreshRequested()
    signal testRequested()
    signal modelSelected(string modelName)
    signal ttsToggledRequested(bool enabled)
    signal memoryToggledRequested(bool enabled)
    signal completeRequested()

    color: Colors.panelBg
    radius: 14
    border.width: 1
    border.color: Colors.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "Welcome to Senticli"
            color: Colors.textPrimary
            font.family: Typography.uiFamily
            font.pixelSize: Typography.titleSize
        }

        Label {
            text: "Pick your local provider, endpoint, and model to get started."
            color: Colors.textSecondary
            wrapMode: Text.Wrap
            font.family: Typography.monoFamily
            font.pixelSize: Typography.smallSize
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true

            ComboBox {
                id: providerCombo
                Layout.preferredWidth: 180
                model: root.providers
                onActivated: root.providerSelected(currentText)
            }

            TextField {
                id: endpointField
                Layout.fillWidth: true
                placeholderText: "192.168.1.220/v1/chat/completions"
                text: root.endpoint
                font.family: Typography.monoFamily
                onAccepted: root.endpointSubmitted(text)
            }

            Button {
                text: "Set"
                onClicked: root.endpointSubmitted(endpointField.text)
            }
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: modelsEndpointField
                Layout.fillWidth: true
                placeholderText: "Optional models endpoint override"
                text: root.modelsEndpoint
                font.family: Typography.monoFamily
                onAccepted: root.modelsEndpointSubmitted(text)
            }

            Button {
                text: "Save Models URL"
                onClicked: root.modelsEndpointSubmitted(modelsEndpointField.text)
            }
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: apiKeyField
                Layout.fillWidth: true
                placeholderText: "Optional API key / bearer token"
                echoMode: TextInput.Password
                text: root.apiKey
                font.family: Typography.monoFamily
                onAccepted: root.apiKeySubmitted(text)
            }

            Button {
                text: "Save Key"
                onClicked: root.apiKeySubmitted(apiKeyField.text)
            }
        }

        RowLayout {
            Layout.fillWidth: true

            ComboBox {
                id: modelCombo
                Layout.fillWidth: true
                model: root.availableModels
                onActivated: root.modelSelected(currentText)
            }

            Button {
                text: "Refresh"
                onClicked: root.refreshRequested()
            }

            Button {
                text: "Test"
                onClicked: root.testRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            Switch {
                id: ttsSwitch
                text: "Enable Voice Output"
                checked: root.ttsEnabled
                onToggled: root.ttsToggledRequested(checked)
            }

            Switch {
                id: memorySwitch
                text: "Enable Project Memory"
                checked: root.memoryEnabled
                onToggled: root.memoryToggledRequested(checked)
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "You can change all of this later in AI Settings."
                color: Colors.textSecondary
                font.family: Typography.monoFamily
                font.pixelSize: Typography.smallSize
                Layout.fillWidth: true
            }

            Button {
                text: "Finish Setup"
                highlighted: true
                onClicked: root.completeRequested()
            }
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

    onModelsEndpointChanged: {
        modelsEndpointField.text = modelsEndpoint
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

    onTtsEnabledChanged: {
        ttsSwitch.checked = ttsEnabled
    }

    onMemoryEnabledChanged: {
        memorySwitch.checked = memoryEnabled
    }

    Component.onCompleted: {
        let idx = providerCombo.find(provider)
        if (idx >= 0) {
            providerCombo.currentIndex = idx
        }
        modelsEndpointField.text = modelsEndpoint
        apiKeyField.text = apiKey
        idx = modelCombo.find(selectedModel)
        if (idx >= 0) {
            modelCombo.currentIndex = idx
        }
    }
}
