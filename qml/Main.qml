import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

ApplicationWindow {
    id: window
    width: 1200
    height: 780
    visible: true
    title: "Senticli"
    color: Colors.windowBg

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        FaceView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(window.height * 0.38)
            faceState: backend.faceState
            statusText: backend.statusText
            micActive: backend.micActive
            speakingActive: backend.speakingActive
        }

        StatusStrip {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            mode: backend.mode
            modelName: backend.modelName
            micActive: backend.micActive
            speakingActive: backend.speakingActive
            actionRunning: backend.actionRunning
            folderScope: backend.folderScope
            modelStatus: backend.modelStatus
            onModeSelected: function(nextMode) {
                backend.setMode(nextMode)
            }
            onSettingsRequested: {
                settingsDialog.open()
            }
        }

        TerminalView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            messageModel: backend.messageModel
            actionRunning: backend.actionRunning
            pendingApproval: backend.pendingApproval
            pendingApprovalText: backend.pendingApprovalText
            onInputSubmitted: function(text) {
                backend.sendUserInput(text)
            }
            onMicToggled: {
                backend.toggleMic()
            }
            onCancelRequested: {
                backend.cancelActiveRequest()
            }
            onApprovalDecided: function(approved) {
                backend.approvePendingAction(approved)
            }
        }
    }

    Popup {
        id: settingsDialog
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(860, window.width - 40)
        height: Math.min(660, window.height - 40)
        padding: 0

        background: Rectangle {
            color: "transparent"
        }

        contentItem: SettingsView {
            provider: backend.provider
            providers: backend.providers
            connectionProfiles: backend.connectionProfiles
            activeProfile: backend.activeProfile
            endpoint: backend.endpoint
            modelsEndpoint: backend.modelsEndpoint
            apiKey: backend.apiKey
            availableModels: backend.availableModels
            selectedModel: backend.selectedModel
            modelStatus: backend.modelStatus
            smoothingProfile: backend.smoothingProfile
            smoothingProfiles: backend.smoothingProfiles
            tokenRate: backend.tokenRate
            assistantName: backend.assistantName
            wakeEnabled: backend.wakeEnabled
            wakeResponses: backend.wakeResponses
            conversationalMode: backend.conversationalMode
            personality: backend.personality
            personalities: backend.personalities
            gender: backend.gender
            genders: backend.genders
            voiceStyle: backend.voiceStyle
            voiceStyles: backend.voiceStyles
            ttsEnabled: backend.ttsEnabled
            memoryEnabled: backend.memoryEnabled
            grantedFolders: backend.grantedFolders
            auditEntries: backend.auditEntries
            setupComplete: backend.setupComplete
            streamingActive: backend.streamingActive
            onProviderSelected: function(name) {
                backend.setProvider(name)
            }
            onActiveProfileSelected: function(name) {
                backend.setActiveProfile(name)
            }
            onSaveProfileRequested: function(name) {
                backend.saveCurrentProfile(name)
            }
            onDeleteProfileRequested: function(name) {
                backend.deleteProfile(name)
            }
            onEndpointSubmitted: function(value) {
                backend.setEndpoint(value)
            }
            onModelsEndpointSubmitted: function(value) {
                backend.setModelsEndpoint(value)
            }
            onApiKeySubmitted: function(value) {
                backend.setApiKey(value)
            }
            onRefreshRequested: {
                backend.refreshModels()
            }
            onTestRequested: {
                backend.testProviderConnection()
            }
            onModelSelected: function(modelName) {
                backend.setSelectedModel(modelName)
            }
            onSmoothingSelected: function(profile) {
                backend.setSmoothingProfile(profile)
            }
            onTokenRateSelected: function(rate) {
                backend.setTokenRate(rate)
            }
            onAssistantNameSubmitted: function(value) {
                backend.setAssistantName(value)
            }
            onWakeEnabledToggled: function(enabled) {
                backend.setWakeEnabled(enabled)
            }
            onWakeResponsesSubmitted: function(responses) {
                backend.setWakeResponses(responses)
            }
            onConversationalModeToggled: function(enabled) {
                backend.setConversationalMode(enabled)
            }
            onPersonalitySelected: function(value) {
                backend.setPersonality(value)
            }
            onGenderSelected: function(value) {
                backend.setGender(value)
            }
            onVoiceStyleSelected: function(value) {
                backend.setVoiceStyle(value)
            }
            onTtsToggledRequested: function(enabled) {
                backend.setTtsEnabled(enabled)
            }
            onMemoryToggledRequested: function(enabled) {
                backend.setMemoryEnabled(enabled)
            }
            onAddFolderRequested: function(folder) {
                backend.addGrantedFolder(folder)
            }
            onRemoveFolderRequested: function(folder) {
                backend.removeGrantedFolder(folder)
            }
            onClearAuditRequested: {
                backend.clearAuditEntries()
            }
            onCompleteSetupRequested: {
                backend.completeSetup()
                setupWizard.close()
            }
            onCloseRequested: {
                settingsDialog.close()
            }
        }
    }

    Popup {
        id: setupWizard
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(860, window.width - 40)
        height: Math.min(420, window.height - 80)
        padding: 0
        visible: !backend.setupComplete

        background: Rectangle {
            color: "transparent"
        }

        contentItem: SetupWizard {
            provider: backend.provider
            providers: backend.providers
            endpoint: backend.endpoint
            modelsEndpoint: backend.modelsEndpoint
            apiKey: backend.apiKey
            availableModels: backend.availableModels
            selectedModel: backend.selectedModel
            ttsEnabled: backend.ttsEnabled
            memoryEnabled: backend.memoryEnabled
            onProviderSelected: function(name) {
                backend.setProvider(name)
            }
            onEndpointSubmitted: function(value) {
                backend.setEndpoint(value)
            }
            onModelsEndpointSubmitted: function(value) {
                backend.setModelsEndpoint(value)
            }
            onApiKeySubmitted: function(value) {
                backend.setApiKey(value)
            }
            onRefreshRequested: {
                backend.refreshModels()
            }
            onTestRequested: {
                backend.testProviderConnection()
            }
            onModelSelected: function(modelName) {
                backend.setSelectedModel(modelName)
            }
            onTtsToggledRequested: function(enabled) {
                backend.setTtsEnabled(enabled)
            }
            onMemoryToggledRequested: function(enabled) {
                backend.setMemoryEnabled(enabled)
            }
            onCompleteRequested: {
                backend.completeSetup()
                setupWizard.close()
            }
        }
    }

    Component.onCompleted: {
        if (!backend.setupComplete) {
            setupWizard.open()
        }
    }
}
