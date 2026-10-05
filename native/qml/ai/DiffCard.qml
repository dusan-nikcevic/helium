import QtQuick
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root
    property var session: Session
    property bool expanded: false
    visible: root.session.stagedAiPlan.length > 0 || root.session.commandBusy
    function channelFor(change) {
        var target = change.target || {}
        var id = target.routeId || target.id || change.trackId || change.channelId || ""
        var project = root.session.project
        var channels = (project.tracks || []).concat(project.buses || [], project.returns || [], [project.master || {}])
        for (var i = 0; i < channels.length; ++i) if (channels[i].id === id || channels[i].engineId === id) return channels[i]
        return {name: id || "Selection", color: ""}
    }
    function valueLabel(change, side) {
        if (change[side + "Display"] !== undefined) return String(change[side + "Display"])
        var value = change[side]
        return typeof value === "object" ? JSON.stringify(value) : String(value)
    }
    implicitHeight: contents.implicitHeight
    color: "#0e0e10"; radius: 13; border.color: "#17ffffff"
    ColumnLayout {
        id: contents
        width: parent.width; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 12; spacing: 8
            Text { textFormat: Text.PlainText; text: root.session.stagedAiPlan.length + (root.session.stagedAiPlan.length === 1 ? " change" : " changes"); color: Shared.Theme.ink; font.pixelSize: 11; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            Text { textFormat: Text.PlainText; text: root.session.aiState === "applied" ? "Applied" : root.session.aiState === "preview" ? "Preview" : root.session.commandBusy ? "Planning" : "Proposed"; color: Shared.Theme.accent; font.pixelSize: 10 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Shared.Theme.border }
        Repeater {
            model: root.session.stagedAiPlan
            RowLayout {
                required property var modelData
                required property int index
                readonly property var planned: modelData
                readonly property var channel: root.channelFor(modelData)
                Layout.fillWidth: true; Layout.leftMargin: 13; Layout.rightMargin: 13; Layout.topMargin: 8; Layout.bottomMargin: 8; spacing: 9
                Rectangle { width: 9; height: 9; radius: 2; color: Shared.Theme.trackColor(parent.channel.color || "") }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Text { textFormat: Text.PlainText; text: parent.parent.channel.name; color: "#e8e8ea"; font.pixelSize: root.expanded ? 12 : 11; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { textFormat: Text.PlainText; text: modelData.label || modelData.description || modelData.property || modelData.kind || "Parameter"; color: Shared.Theme.muted; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text {
                        textFormat: Text.PlainText
                        visible: (root.expanded || root.session.aiState === "preview") && !!parent.parent.planned
                        text: {
                            const change = parent.parent.planned
                            if (!change) return ""
                            return root.valueLabel(change, "before") + " → " + root.valueLabel(change, "after")
                        }
                        color: Shared.Theme.secondary; font.family: Shared.Theme.monoFamily; font.pixelSize: 10
                    }
                }

            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Shared.Theme.border }
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 12; spacing: 8
            Shared.DawButton { objectName: "ai-preview"; Layout.fillWidth: true; text: root.session.aiState === "preview" ? "Previewing" : "Preview"; checked: root.session.aiState === "preview"; radius: 16; enabled: !root.session.commandBusy && root.session.aiState !== "applied" && root.session.stagedAiPlan.length > 0; tooltip: "Inspect proposed parameter changes"; onClicked: root.session.previewAi() }
            Shared.DawButton { objectName: "ai-undo"; Layout.fillWidth: true; text: "Undo"; radius: 16; enabled: !root.session.commandBusy && root.session.canUndo; onClicked: root.session.undo() }
            Shared.DawButton { objectName: "ai-apply"; Layout.fillWidth: true; text: root.session.aiState === "applied" ? "Applied ✓" : "Apply"; radius: 16; accent: true; tint: root.session.aiState === "applied" ? Shared.Theme.success : Shared.Theme.accent; enabled: !root.session.commandBusy && root.session.aiState !== "applied" && root.session.stagedAiPlan.length > 0; onClicked: root.session.applyAi() }
        }
    }
}
