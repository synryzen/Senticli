import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    required property var messageModel
    property bool actionRunning: false
    property bool micActive: false
    property bool pendingApproval: false
    property string pendingApprovalText: ""

    signal inputSubmitted(string text)
    signal micToggled()
    signal cancelRequested()
    signal approvalDecided(bool approved)

    color: "#090D10"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0B1115"
            border.width: 1
            border.color: "#2A373E"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: "#0F161B"
                    border.width: 1
                    border.color: "#2A373E"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Rectangle { width: 10; height: 10; radius: 5; color: "#FF5F56" }
                        Rectangle { width: 10; height: 10; radius: 5; color: "#FFBD2E" }
                        Rectangle { width: 10; height: 10; radius: 5; color: "#27C93F" }

                        Label {
                            text: "senticli terminal session"
                            color: Colors.textSecondary
                            font.family: Typography.monoFamily
                            font.pixelSize: Typography.smallSize
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: root.actionRunning ? "RUNNING" : "IDLE"
                            color: root.actionRunning ? Colors.warning : Colors.textSecondary
                            font.family: Typography.monoFamily
                            font.pixelSize: Typography.smallSize
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0A0F13"

                    ListView {
                        id: messageList
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 2
                        clip: true
                        model: root.messageModel
                        delegate: CommandBubble {}

                        onCountChanged: positionViewAtEnd()
                        Component.onCompleted: positionViewAtEnd()
                    }
                }
            }
        }

        Rectangle {
            visible: root.pendingApproval
            Layout.fillWidth: true
            color: "#201714"
            border.width: 1
            border.color: "#AA6A54"
            radius: 6
            implicitHeight: actions.implicitHeight + 16

            ColumnLayout {
                id: actions
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Label {
                    text: "CONFIRM ACTION: " + root.pendingApprovalText
                    wrapMode: Text.Wrap
                    color: "#F2C4B3"
                    font.family: Typography.monoFamily
                    font.pixelSize: Typography.bodySize
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight

                    Button {
                        text: "No"
                        onClicked: root.approvalDecided(false)
                    }

                    Button {
                        text: "Yes"
                        highlighted: true
                        onClicked: root.approvalDecided(true)
                    }
                }
            }
        }

        InputBar {
            Layout.fillWidth: true
            actionRunning: root.actionRunning
            micActive: root.micActive
            promptText: "matthew@senticli:~$"
            onSubmitted: function(text) {
                root.inputSubmitted(text)
            }
            onMicToggled: {
                root.micToggled()
            }
            onCancelRequested: {
                root.cancelRequested()
            }
        }
    }
}
