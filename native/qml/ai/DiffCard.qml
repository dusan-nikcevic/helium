import QtQuick
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root
    property var session: Session
    property bool expanded: false
    implicitHeight: contents.implicitHeight
    color: "#0e0e10"; radius: 13; border.color: "#17ffffff"
    ColumnLayout {
        id: contents
        width: parent.width; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 12; spacing: 8
            Text { text: root.session.project.diff.title; color: Shared.Theme.ink; font.pixelSize: 11; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            Text { text: root.session.aiState === "applied" ? "Applied" : root.session.aiState === "preview" ? "Preview" : root.session.project.diff.section; color: Shared.Theme.accent; font.pixelSize: 10 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Shared.Theme.border }
        Repeater {
            model: root.session.project.diff.changes
            RowLayout {
                required property var modelData
                required property int index
                readonly property var planned: root.session.stagedAiPlan[index]
                Layout.fillWidth: true; Layout.leftMargin: 13; Layout.rightMargin: 13; Layout.topMargin: 8; Layout.bottomMargin: 8; spacing: 9
                Rectangle { width: 9; height: 9; radius: 2; color: Shared.Theme.trackColor(modelData.color) }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Text { text: modelData.trackName; color: "#e8e8ea"; font.pixelSize: root.expanded ? 12 : 11; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: modelData.description; color: Shared.Theme.muted; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text {
                        visible: (root.expanded || root.session.aiState === "preview") && !!parent.parent.planned
                        text: {
                            const change = parent.parent.planned
                            if (!change) return ""
                            return (change.beforeDisplay || change.before + "%") + " → " + (change.afterDisplay || change.after + "%")
                        }
                        color: Shared.Theme.secondary; font.family: Shared.Theme.monoFamily; font.pixelSize: 10
                    }
                }
                Text { text: modelData.delta; color: modelData.direction === "up" ? Shared.Theme.success : "#e8856f"; font.family: Shared.Theme.monoFamily; font.pixelSize: 10; font.weight: Font.DemiBold }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Shared.Theme.border }
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 12; spacing: 8
            Shared.DawButton { objectName: "ai-preview"; Layout.fillWidth: true; text: root.session.aiState === "preview" ? "Previewing" : "Preview"; checked: root.session.aiState === "preview"; radius: 16; enabled: root.session.aiState !== "applied" && root.session.stagedAiPlan.length > 0; tooltip: "Inspect proposed demo parameters; no audio audition"; onClicked: root.session.previewAi() }
            Shared.DawButton { objectName: "ai-undo"; Layout.fillWidth: true; text: "Undo"; radius: 16; enabled: root.session.canUndo; onClicked: root.session.undo() }
            Shared.DawButton { objectName: "ai-apply"; Layout.fillWidth: true; text: root.session.aiState === "applied" ? "Applied ✓" : "Apply"; radius: 16; accent: true; tint: root.session.aiState === "applied" ? Shared.Theme.success : Shared.Theme.accent; enabled: root.session.aiState !== "applied" && root.session.stagedAiPlan.length > 0; onClicked: root.session.applyAi() }
        }
    }
}
