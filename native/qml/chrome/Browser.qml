import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root
    property var session: Session
    property string category: "Sounds"
    property string query: ""
    color: Shared.Theme.panel
    implicitWidth: 236
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 12; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Browser"; color: Shared.Theme.ink; font.pixelSize: 12; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            Text { text: "⋯"; color: Shared.Theme.dim }
        }
        Rectangle {
            Layout.fillWidth: true; height: 28; radius: 8; color: Shared.Theme.well; border.color: Shared.Theme.border
            RowLayout {
                anchors.fill: parent; anchors.margins: 2; spacing: 1
                Repeater {
                    model: ["Sounds", "Loops", "FX", "Files"]
                    Shared.DawButton { required property string modelData; text: modelData; Layout.fillWidth: true; height: 24; checked: root.category === modelData; tint: Shared.Theme.ink; onClicked: root.category = modelData }
                }
            }
        }
        TextField {
            objectName: "browser-search"
            Layout.fillWidth: true; implicitHeight: 29; leftPadding: 12; rightPadding: 12
            placeholderText: "⌕  Search your library"; placeholderTextColor: Shared.Theme.dim
            color: Shared.Theme.secondary; font.family: Shared.Theme.fontFamily; font.pixelSize: 11
            Accessible.name: "Search library"
            onTextChanged: root.query = text.toLowerCase()
            background: Rectangle { color: Shared.Theme.well; border.color: parent.activeFocus ? Shared.Theme.accent : Shared.Theme.border; radius: 15 }
        }
        Flickable {
            Layout.fillHeight: true; Layout.fillWidth: true; clip: true
            contentHeight: content.implicitHeight; boundsBehavior: Flickable.StopAtBounds
            Column {
                id: content; width: parent.width; spacing: 2
                Text { text: root.category === "Sounds" ? "INSTRUMENTS" : root.category.toUpperCase(); height: 22; verticalAlignment: Text.AlignVCenter; font.pixelSize: 9; font.weight: Font.DemiBold; color: Shared.Theme.dim; font.letterSpacing: .4 }
                Repeater {
                    model: root.session.project.browserInstruments.filter(item => (!root.query || item.name.toLowerCase().includes(root.query)) && (root.category === "Sounds" || (root.category === "FX" && item.tag === "FX")))
                    Rectangle {
                        required property var modelData
                        width: content.width; height: 28; radius: 7; color: libraryHover.hovered ? "#0fffffff" : "transparent"
                        RowLayout {
                            anchors.fill: parent; spacing: 9
                            Rectangle { width: 8; height: 8; radius: 2; color: Shared.Theme.trackColor(modelData.color) }
                            Text { text: modelData.name; Layout.fillWidth: true; font.pixelSize: 12; color: "#d6d6db"; elide: Text.ElideRight }
                            Text { text: modelData.tag; font.pixelSize: 10; color: Shared.Theme.faint }
                        }
                        HoverHandler { id: libraryHover }
                    }
                }
                Text {
                    width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 11; color: Shared.Theme.muted
                    visible: root.category === "Loops" || root.category === "Files" || (root.query && !root.session.project.browserInstruments.some(item => item.name.toLowerCase().includes(root.query)))
                    text: root.query ? "No matching items." : "Import and file browsing require the engine integration."
                }
                Text { text: "✦ AI HISTORY"; height: 32; verticalAlignment: Text.AlignVCenter; font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: .4; color: Shared.Theme.dim }
                Repeater {
                    model: root.session.aiHistory
                    RowLayout {
                        required property var modelData
                        width: content.width; height: 31; spacing: 9
                        Rectangle { width: 6; height: 6; radius: 3; color: modelData.status === "undone" ? Shared.Theme.dim : Shared.Theme.accent }
                        Text { Layout.fillWidth: true; text: modelData.text || modelData.command || "AI edit"; color: "#b6b6bb"; font.pixelSize: 10; elide: Text.ElideRight }
                        Text { text: modelData.time || "now"; font.pixelSize: 9; color: Shared.Theme.faint }
                    }
                }
            }
            ScrollBar.vertical: ScrollBar { width: 5 }
        }
    }
}
