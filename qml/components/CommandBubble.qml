import QtQuick
import QtQuick.Layouts
import Senticli

Item {
    id: root
    required property string source
    required property string text
    required property string kind
    required property string time

    width: ListView.view.width
    height: container.implicitHeight + 10

    RowLayout {
        id: container
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8

        Item {
            Layout.fillWidth: root.source === "user"
        }

        Rectangle {
            id: bubble
            radius: 10
            border.width: 1
            border.color: Colors.border
            color: {
                if (root.kind === "warning") return Colors.warningBubble
                if (root.source === "user") return Colors.userBubble
                if (root.source === "system") return Colors.systemBubble
                return Colors.assistantBubble
            }
            Layout.maximumWidth: parent.width * 0.82
            implicitWidth: Math.min(parent.width * 0.82, content.implicitWidth + 24)
            implicitHeight: content.implicitHeight + 12

            Column {
                id: content
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                spacing: 6

                Text {
                    text: root.text
                    wrapMode: Text.Wrap
                    color: Colors.textPrimary
                    font.family: Typography.monoFamily
                    font.pixelSize: Typography.bodySize
                }

                Text {
                    text: root.time
                    color: Colors.textSecondary
                    font.family: Typography.monoFamily
                    font.pixelSize: Typography.smallSize
                    horizontalAlignment: Text.AlignRight
                    width: parent.width
                }
            }
        }

        Item {
            Layout.fillWidth: root.source !== "user"
        }
    }
}
