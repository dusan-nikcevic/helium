import QtQuick
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root
    property var session: Session
    property bool browserVisible: true
    property bool inspectorVisible: true
    signal toggleBrowser()
    signal toggleInspector()
    implicitHeight: 40
    color: "#141416"
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Shared.Theme.border }
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 14
        Rectangle {
            width: 256; height: 31; radius: 9; color: Shared.Theme.well; border.color: Shared.Theme.border
            Row {
                anchors.centerIn: parent; spacing: 2
                Repeater {
                    model: ["arrange", "mix", "split", "session"]
                    Shared.DawButton {
                        required property string modelData
                        objectName: "view-" + modelData
                        text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                        width: 61; height: 25; checked: root.session.view === modelData; tint: Shared.Theme.ink
                        onClicked: root.session.setView(modelData)
                    }
                }
            }
        }
        Rectangle { width: 1; height: 18; color: Shared.Theme.border }
        Text { text: root.session.project.tracks.length + " tracks  ·  " + root.session.project.buses.length + " buses"; color: Shared.Theme.muted; font.pixelSize: 11 }
        Item { Layout.fillWidth: true }
        Shared.DawButton { text: "Browser"; checked: root.browserVisible; tooltip: "Show or hide library browser"; visible: root.session.view === "arrange" || root.session.view === "session"; onClicked: root.toggleBrowser() }
        Shared.DawButton { text: "Inspector"; checked: root.inspectorVisible; tooltip: "Show or hide inspector"; visible: root.session.view === "arrange" || root.session.view === "session"; onClicked: root.toggleInspector() }
        Shared.DawButton { text: root.session.snapEnabled ? "Snap: 1/4" : "Snap: Off"; checked: root.session.snapEnabled; onClicked: root.session.toggleSnap() }
        Shared.DawButton { text: "Automation"; checked: root.session.automationVisible; onClicked: root.session.toggleAutomation() }
    }
}
