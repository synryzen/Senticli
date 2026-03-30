import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    property string mode: "Assist"
    property string modelName: "Local Model"
    property bool micActive: false
    property bool speakingActive: false
    property bool actionRunning: false
    property string folderScope: "~/Documents"
    property string modelStatus: ""

    signal modeSelected(string mode)
    signal settingsRequested()

    color: Colors.panelElevated
    border.width: 1
    border.color: Colors.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Label {
            text: "Mode"
            color: Colors.textSecondary
            font.family: Typography.uiFamily
            font.pixelSize: Typography.smallSize
        }

        Repeater {
            model: ["Chat", "Assist", "Command"]
            delegate: Button {
                text: modelData
                highlighted: root.mode === modelData
                onClicked: root.modeSelected(modelData)
                font.family: Typography.uiFamily
                font.pixelSize: Typography.smallSize
            }
        }

        Item { Layout.fillWidth: true }

        Label {
            text: "Model: " + root.modelName
            color: Colors.textSecondary
            font.family: Typography.monoFamily
            font.pixelSize: Typography.smallSize
        }

        Label {
            text: root.micActive ? "Mic On" : "Mic Off"
            color: root.micActive ? Colors.success : Colors.textSecondary
            font.family: Typography.monoFamily
            font.pixelSize: Typography.smallSize
        }

        Label {
            text: root.speakingActive ? "Speaking" : "Quiet"
            color: root.speakingActive ? Colors.info : Colors.textSecondary
            font.family: Typography.monoFamily
            font.pixelSize: Typography.smallSize
        }

        Label {
            text: root.actionRunning ? "Busy" : "Idle"
            color: root.actionRunning ? Colors.warning : Colors.textSecondary
            font.family: Typography.monoFamily
            font.pixelSize: Typography.smallSize
        }

        Label {
            text: root.modelStatus
            color: Colors.textSecondary
            font.family: Typography.monoFamily
            font.pixelSize: Typography.smallSize
            elide: Label.ElideRight
            Layout.maximumWidth: 220
        }

        Label {
            text: "Scope: " + root.folderScope
            color: Colors.textSecondary
            font.family: Typography.monoFamily
            font.pixelSize: Typography.smallSize
            elide: Label.ElideRight
            Layout.maximumWidth: 260
        }

        Button {
            text: "AI Settings"
            highlighted: true
            onClicked: root.settingsRequested()
            font.family: Typography.uiFamily
            font.pixelSize: Typography.smallSize
        }
    }
}
