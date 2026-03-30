import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    property bool actionRunning: false
    property string promptText: "user@senticli:~$"
    property var commandHistory: []
    property int historyIndex: 0
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
        if (root.commandHistory.length === 0 || root.commandHistory[root.commandHistory.length - 1] !== value) {
            root.commandHistory.push(value)
        }
        root.historyIndex = root.commandHistory.length
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
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Up) {
                    if (root.commandHistory.length === 0) {
                        return
                    }
                    root.historyIndex = Math.max(0, root.historyIndex - 1)
                    input.text = root.commandHistory[root.historyIndex]
                    input.cursorPosition = input.text.length
                    event.accepted = true
                } else if (event.key === Qt.Key_Down) {
                    if (root.commandHistory.length === 0) {
                        return
                    }
                    root.historyIndex = Math.min(root.commandHistory.length, root.historyIndex + 1)
                    if (root.historyIndex === root.commandHistory.length) {
                        input.clear()
                    } else {
                        input.text = root.commandHistory[root.historyIndex]
                        input.cursorPosition = input.text.length
                    }
                    event.accepted = true
                }
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
