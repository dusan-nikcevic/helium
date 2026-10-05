import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Popup {
    id: root
    property var session: Session
    width: Math.min(600, parent.width - 80)
    x: (parent.width - width) / 2; y: 96
    modal: true; focus: true; padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: input.forceActiveFocus()
    Overlay.modal: Rectangle { color: "#9408080a" }
    background: Rectangle { radius: 18; color: "#ee1c1c20"; border.color: "#24ffffff" }
    contentItem: ColumnLayout {
        spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 18; spacing: 12
            Rectangle { width: 24; height: 24; radius: 7; color: Shared.Theme.accent; Text { anchors.centerIn: parent; text: "✦"; color: "white" } }
            TextField {
                id: input; objectName: "command-input"
                Layout.fillWidth: true; color: Shared.Theme.ink; font.pixelSize: 15
                text: root.session.requestText || "Clean up the vocal chain but keep it natural"
                Accessible.name: "AI command"
                background: Item {}
                onAccepted: root.session.submitAi(text)
            }
            Shared.DawButton { text: "↵"; tooltip: "Propose demo command"; onClicked: root.session.submitAi(input.text) }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Shared.Theme.border }
        Text { Layout.margins: 18; text: "● Suggested  ·  " + (root.session.selectedTrack.name || "Demo session"); color: Shared.Theme.success; font.pixelSize: 11 }
        DiffCard { session: root.session; expanded: true; Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20 }
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 20
            Text { Layout.fillWidth: true; text: "Demo parameters · Esc to dismiss"; color: Shared.Theme.dim; font.pixelSize: 10 }
            Shared.DawButton { text: "Discard"; radius: 16; onClicked: { root.session.discardAi(); root.close() } }
            Shared.DawButton { text: "Close"; radius: 16; onClicked: root.close() }
        }
        Text { Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20; Layout.bottomMargin: 16; text: root.session.notice; color: Shared.Theme.muted; font.pixelSize: 10; wrapMode: Text.WordWrap }
    }
}
