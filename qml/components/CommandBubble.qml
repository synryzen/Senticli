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

    function escapeHtml(value) {
        return value
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#39;")
    }

    function ansiColor(code, fallback) {
        const map = {
            30: "#2E3440", 31: "#E06C75", 32: "#98C379", 33: "#E5C07B", 34: "#61AFEF", 35: "#C678DD", 36: "#56B6C2", 37: "#D8DEE9",
            90: "#4C566A", 91: "#FF6B6B", 92: "#A3E635", 93: "#FACC15", 94: "#93C5FD", 95: "#E879F9", 96: "#67E8F9", 97: "#ECEFF4"
        }
        if (map[code] !== undefined) {
            return map[code]
        }
        return fallback
    }

    function ansiToRichText(value, fallbackColor) {
        const cleaned = value
            .replace(/\r/g, "")
            .replace(/\x1b\][^\x07]*(\x07|\x1b\\)/g, "")

        const regex = /\x1b\[([0-9;]*)m/g
        let result = ""
        let last = 0
        let color = fallbackColor
        let bold = false
        let open = false

        function closeSpan() {
            if (open) {
                result += "</span>"
                open = false
            }
        }

        function openSpan() {
            const styles = []
            if (color && color.length > 0) {
                styles.push("color:" + color)
            }
            if (bold) {
                styles.push("font-weight:600")
            }
            if (styles.length > 0) {
                result += "<span style=\"" + styles.join(";") + "\">"
                open = true
            }
        }

        openSpan()
        let match
        while ((match = regex.exec(cleaned)) !== null) {
            const plain = cleaned.slice(last, match.index)
            result += escapeHtml(plain).replace(/\n/g, "<br/>")

            const codes = match[1].length > 0 ? match[1].split(";") : ["0"]
            for (let i = 0; i < codes.length; i++) {
                const c = parseInt(codes[i], 10)
                if (c === 0) {
                    color = fallbackColor
                    bold = false
                } else if (c === 1) {
                    bold = true
                } else if ((c >= 30 && c <= 37) || (c >= 90 && c <= 97)) {
                    color = ansiColor(c, fallbackColor)
                } else if (c === 39) {
                    color = fallbackColor
                } else if (c === 22) {
                    bold = false
                }
            }

            closeSpan()
            openSpan()
            last = regex.lastIndex
        }

        const tail = cleaned.slice(last).replace(/\x1b\[[0-9;?]*[A-Za-z]/g, "")
        result += escapeHtml(tail).replace(/\n/g, "<br/>")
        closeSpan()
        return result
    }

    function richLine() {
        return ansiToRichText(formattedLine(), lineColor.toString())
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
            text: root.richLine()
            textFormat: Text.RichText
            wrapMode: Text.Wrap
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
