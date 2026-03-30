import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    property bool actionRunning: false
    signal submitted(string text)
    signal micToggled()
    signal cancelRequested()

    color: Colors.panelElevated
    border.width: 1
    border.color: Colors.border
    radius: 10
    implicitHeight: 54

    function submitText() {
        const value = input.text.trim()
        if (value.length === 0) {
            return
        }
        root.submitted(value)
        input.clear()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        TextField {
            id: input
            Layout.fillWidth: true
            placeholderText: "Ask, command, or use slash commands (/help)"
            font.family: Typography.monoFamily
            font.pixelSize: Typography.bodySize
            color: Colors.textPrimary
            onAccepted: root.submitText()
        }

        Button {
            text: "Mic"
            onClicked: root.micToggled()
            font.family: Typography.uiFamily
            font.pixelSize: Typography.smallSize
        }

        Button {
            visible: root.actionRunning
            text: "Cancel"
            onClicked: root.cancelRequested()
            font.family: Typography.uiFamily
            font.pixelSize: Typography.smallSize
        }

        Button {
            text: "Send"
            onClicked: root.submitText()
            font.family: Typography.uiFamily
            font.pixelSize: Typography.smallSize
        }
    }
}
