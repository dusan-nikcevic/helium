import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root
    property var session: Session
    property bool forceTrackMode: false
    property string panel: "assistant"
    signal openDevices(string trackId)
    color: Shared.Theme.panel
    implicitWidth: 328
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 48; Layout.leftMargin: 16; Layout.rightMargin: 12; spacing: 8
            Rectangle {
                width: 22; height: 22; radius: 7; color: root.forceTrackMode || root.panel === "track" ? Shared.Theme.trackColor(root.session.selectedTrack.color || "master") : Shared.Theme.accent
                Text { anchors.centerIn: parent; text: root.forceTrackMode || root.panel === "track" ? "⌁" : "✦"; color: "white"; font.pixelSize: 11 }
            }
            Text { text: root.session.engineConnected || root.forceTrackMode || root.panel === "track" ? root.session.selectedTrack.name || "Track inspector" : root.panel === "history" ? "AI History" : "Assistant"; Layout.fillWidth: true; color: Shared.Theme.ink; font.pixelSize: 12; font.weight: Font.DemiBold }
            Shared.DawButton { text: "⌁"; implicitWidth: 25; tooltip: "Selected track inspector"; checked: root.panel === "track"; onClicked: root.panel = root.panel === "track" ? "assistant" : "track" }
            Shared.DawButton { text: "↶"; implicitWidth: 25; tooltip: "AI history"; checked: root.panel === "history"; onClicked: root.panel = root.panel === "history" ? "assistant" : "history" }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Shared.Theme.border }
        Loader {
            Layout.fillWidth: true; Layout.fillHeight: true
            sourceComponent: root.session.engineConnected || root.forceTrackMode || root.panel === "track" ? trackInspector : root.panel === "history" ? historyPanel : assistantPanel
        }
    }
    Component {
        id: assistantPanel
        ColumnLayout {
            spacing: 14
            Flickable {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentHeight: conversation.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ColumnLayout {
                    id: conversation; width: parent.width; spacing: 14
                    Rectangle {
                        Layout.fillWidth: true; Layout.leftMargin: 52; Layout.rightMargin: 16; Layout.topMargin: 16
                        implicitHeight: userText.implicitHeight + 22; radius: 14; color: "#281084ff"; border.color: "#482997ff"
                        Text { id: userText; anchors.fill: parent; anchors.margins: 11; text: root.session.requestText || root.session.project.chat[0].text; wrapMode: Text.WordWrap; color: "#e9f2ff"; font.pixelSize: 12; font.family: Shared.Theme.fontFamily; lineHeight: 1.3 }
                    }
                    RowLayout {
                        Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16; spacing: 9
                        Rectangle { Layout.alignment: Qt.AlignTop; width: 22; height: 22; radius: 7; color: Shared.Theme.accent; Text { anchors.centerIn: parent; text: "✦"; color: "white"; font.pixelSize: 10 } }
                        Text { Layout.fillWidth: true; text: "Here are the proposed changes for the demo session. Review the parameters before applying."; wrapMode: Text.WordWrap; color: Shared.Theme.secondary; font.family: Shared.Theme.fontFamily; font.pixelSize: 12; lineHeight: 1.4 }
                    }
                    DiffCard { session: root.session; Layout.fillWidth: true; Layout.leftMargin: 47; Layout.rightMargin: 16 }
                    Text {
                        Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16
                        text: root.session.notice; color: Shared.Theme.muted; wrapMode: Text.WordWrap; font.pixelSize: 10
                    }
                }
                ScrollBar.vertical: ScrollBar { width: 5 }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.bottomMargin: 14; spacing: 9
                Flow {
                    Layout.fillWidth: true; spacing: 6
                    Repeater {
                        model: root.session.project.suggestionChips
                        Shared.DawButton { required property string modelData; text: modelData; height: 23; radius: 12; onClicked: composer.text = modelData }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; height: 43; radius: 22; color: Shared.Theme.well; border.color: composer.activeFocus ? Shared.Theme.accent : "#20ffffff"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 7; spacing: 8
                        TextField {
                            id: composer; objectName: "ai-composer"; Layout.fillWidth: true
                            placeholderText: "Describe an edit…"; placeholderTextColor: Shared.Theme.dim
                            color: Shared.Theme.ink; font.pixelSize: 12; leftPadding: 7; rightPadding: 0
                            Accessible.name: "Describe an edit to the demo session"
                            background: Item {}
                            onAccepted: root.session.submitAi(text)
                        }
                        Shared.DawButton { text: "↑"; implicitWidth: 28; height: 28; radius: 14; accent: true; tooltip: "Propose demo edit"; enabled: composer.text.trim().length > 0; onClicked: root.session.submitAi(composer.text) }
                    }
                }
            }
        }
    }
    Component {
        id: trackInspector
        Flickable {
            contentHeight: trackContents.implicitHeight + 32; clip: true; boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: trackContents; width: parent.width - 32; x: 16; y: 16; spacing: 18
                Text { text: root.session.selectedClip.name ? root.session.selectedClip.name + " · " + root.session.selectedClip.type : "Select a track or clip"; color: Shared.Theme.muted; font.pixelSize: 11 }
                RowLayout {
                    Layout.fillWidth: true; spacing: 14
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 7
                        Text { text: "VOLUME  " + Number(root.session.selectedTrack.volumeDb || 0).toFixed(1) + " dB"; color: Shared.Theme.secondary; font.pixelSize: 10 }
                        Slider {
                            Layout.fillWidth: true; from: -60; to: 6; value: root.session.selectedTrack.volumeDb || 0
                            Accessible.name: "Track volume"
                            onPressedChanged: pressed ? root.session.beginGesture() : root.session.endGesture()
                            onMoved: root.session.setTrackValue(root.session.selectedTrackId, "volumeDb", value)
                        }
                    }
                    Shared.Knob { enabled: !root.session.engineConnected || !!root.session.selectedTrack.panAvailable; value: ((root.session.selectedTrack.pan || 0) + 1) / 2; label: "PAN"; onValueEdited: value => root.session.setTrackValue(root.session.selectedTrackId, "pan", value * 2 - 1) }
                }
                Text { text: "DEVICE CHAIN"; color: Shared.Theme.dim; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: .4 }
                Repeater {
                    model: root.session.selectedTrack.devices || []
                    Shared.DawButton {
                        required property var modelData
                        Layout.fillWidth: true; height: 35; text: (modelData.enabled ? "● " : "○ ") + modelData.name + (modelData.aiEdited ? "   ✦" : "")
                        checked: !!modelData.aiEdited; onClicked: root.openDevices(root.session.selectedTrackId)
                    }
                }
                Text { text: "SENDS"; color: Shared.Theme.dim; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: .4 }
                Repeater {
                    model: root.session.selectedTrack.sends || []
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Text { text: modelData.name; color: Shared.Theme.secondary; font.pixelSize: 11; Layout.fillWidth: true }
                        Text { text: Math.round(modelData.amount) + "%"; color: Shared.Theme.success; font.family: Shared.Theme.monoFamily; font.pixelSize: 11 }
                    }
                }
                Item { height: 8 }
            }
            ScrollBar.vertical: ScrollBar { width: 5 }
        }
    }
    Component {
        id: historyPanel
        ListView {
            model: root.session.aiHistory; spacing: 8; clip: true
            header: Text { width: parent.width; height: 40; text: "Demo edits and reference history"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: Shared.Theme.muted; font.pixelSize: 11 }
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width - 32; x: 16; height: 60; radius: 9; color: Shared.Theme.well; border.color: Shared.Theme.border
                Column {
                    anchors.fill: parent; anchors.margins: 10; spacing: 5
                    Text { width: parent.width; text: modelData.text || modelData.command || "AI edit"; color: Shared.Theme.secondary; font.pixelSize: 11; elide: Text.ElideRight }
                    Text { text: modelData.reference ? "Design reference · " + modelData.time : modelData.status || "Applied"; font.pixelSize: 10; color: Shared.Theme.muted }
                }
            }
            ScrollBar.vertical: ScrollBar { width: 5 }
        }
    }
}
