import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Senticli

Rectangle {
    id: root
    required property var messageModel
    property bool actionRunning: false
    property bool pendingApproval: false
    property string pendingApprovalText: ""

    signal inputSubmitted(string text)
    signal micToggled()
    signal cancelRequested()
    signal approvalDecided(bool approved)

    color: Colors.windowBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Colors.panelBg
            border.width: 1
            border.color: Colors.border
            radius: 12

            ListView {
                id: messageList
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                clip: true
                model: root.messageModel
                delegate: CommandBubble {}

                onCountChanged: positionViewAtEnd()
                Component.onCompleted: positionViewAtEnd()
            }
        }

        Rectangle {
            visible: root.pendingApproval
            Layout.fillWidth: true
            color: Colors.warningBubble
            border.width: 1
            border.color: Colors.warning
            radius: 10
            implicitHeight: actions.implicitHeight + 16

            ColumnLayout {
                id: actions
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Label {
                    text: root.pendingApprovalText
                    wrapMode: Text.Wrap
                    color: Colors.textPrimary
                    font.family: Typography.monoFamily
                    font.pixelSize: Typography.bodySize
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight

                    Button {
                        text: "Decline"
                        onClicked: root.approvalDecided(false)
                    }

                    Button {
                        text: "Approve"
                        highlighted: true
                        onClicked: root.approvalDecided(true)
                    }
                }
            }
        }

        InputBar {
            Layout.fillWidth: true
            actionRunning: root.actionRunning
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
