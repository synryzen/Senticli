import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    property bool actionRunning: false
    property string promptText: "user@senticli:~$"
    signal submitted(string text)
    signal micToggled()
    signal cancelRequested()

    color: "#0E1419"
    border.width: 1
    border.color: "#2A373E"
    radius: 6
    implicitHeight: 48

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

        Label {
            text: root.promptText
            color: Colors.success
            font.family: Typography.monoFamily
            font.pixelSize: Typography.bodySize
        }

        TextField {
            id: input
            Layout.fillWidth: true
            placeholderText: "type command or question..."
            font.family: Typography.monoFamily
            font.pixelSize: Typography.bodySize
            color: Colors.textPrimary
            background: Rectangle {
                color: "transparent"
                border.width: 0
            }
            onAccepted: root.submitText()
        }

        Button {
            text: "mic"
            onClicked: root.micToggled()
            font.family: Typography.uiFamily
            font.pixelSize: Typography.smallSize
        }

        Button {
            visible: root.actionRunning
            text: "cancel"
            onClicked: root.cancelRequested()
            font.family: Typography.uiFamily
            font.pixelSize: Typography.smallSize
        }

        Button {
            text: "enter"
            onClicked: root.submitText()
            font.family: Typography.uiFamily
            font.pixelSize: Typography.smallSize
        }
    }
}
