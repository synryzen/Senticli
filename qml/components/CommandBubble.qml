import QtQuick
import QtQuick.Layouts
import Senticli

Item {
    id: root
    required property string source
    required property string text
    required property string kind
    required property string time

    width: ListView.view ? ListView.view.width : 800
    height: lineBlock.implicitHeight + 6

    readonly property color lineColor: {
        if (root.kind === "warning") return Colors.warning
        if (root.source === "user") return Colors.success
        if (root.source === "system") return Colors.info
        return Colors.textPrimary
    }

    readonly property string prefix: {
        if (root.kind === "warning") return "[warn] "
        if (root.source === "user") return "matthew@senticli:~$ "
        if (root.source === "system") return "[sys] "
        return "senticli> "
    }

    function formattedLine() {
        const t = (root.text || "")
        if (t.length === 0) {
            return root.prefix
        }
        if (root.source === "user") {
            return root.prefix + t
        }
        return root.prefix + t.replace(/\n/g, "\n" + "  ")
    }

    ColumnLayout {
        id: lineBlock
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        spacing: 2

        Text {
            Layout.fillWidth: true
            text: root.formattedLine()
            wrapMode: Text.Wrap
            color: root.lineColor
            font.family: Typography.monoFamily
            font.pixelSize: Typography.bodySize
        }

        Text {
            visible: false
            text: root.time
            color: Colors.textSecondary
            font.family: Typography.monoFamily
            font.pixelSize: 10
        }
    }
}
