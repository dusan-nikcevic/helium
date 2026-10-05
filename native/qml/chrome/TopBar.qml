import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root
    property var session: Session
    signal assistantRequested()
    signal closeRequested()
    signal minimizeRequested()
    signal maximizeRequested()
    implicitHeight: 48
    color: Shared.Theme.rail
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Shared.Theme.border }
    RowLayout {
        anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 14; anchors.rightMargin: 14; height: 38; spacing: 14
        RowLayout {
            Layout.preferredWidth: root.width < 1300 ? 240 : 300; spacing: 8
            Row {
                spacing: 7
                Repeater {
                    model: ["#ff5f57", "#febc2e", "#28c840"]
                    Rectangle {
                        required property string modelData
                        required property int index
                        width: 11; height: 11; radius: 6; color: modelData
                        MouseArea { anchors.fill: parent; onClicked: index === 0 ? root.closeRequested() : index === 1 ? root.minimizeRequested() : root.maximizeRequested() }
                    }
                }
            }
            Text { text: "Aria"; color: Shared.Theme.ink; font.family: Shared.Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold; Layout.leftMargin: 6 }
            Rectangle { width: 1; height: 16; color: "#20ffffff" }
            Text { text: root.session.project.name; color: Shared.Theme.secondary; font.family: Shared.Theme.fontFamily; font.pixelSize: 12 }
            Text { text: "· " + root.session.project.section; color: Shared.Theme.dim; font.pixelSize: 11 }
            Rectangle {
                width: 36; height: 16; radius: 5; border.color: Shared.Theme.border; color: "transparent"
                Text { anchors.centerIn: parent; text: root.session.engineConnected ? "Engine" : "Demo"; font.pixelSize: 9; color: Shared.Theme.dim }
            }
            Item { Layout.fillWidth: true }
        }
        Item {
            Layout.fillWidth: true; Layout.minimumWidth: 455; Layout.fillHeight: true
            Rectangle {
                anchors.centerIn: parent; width: 455; height: 38; radius: 12; border.color: "#22ffffff"
                gradient: Gradient { GradientStop { position: 0; color: "#101012" } GradientStop { position: 1; color: "#0a0a0c" } }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 3
                    Shared.DawButton { text: "|◀"; implicitWidth: 27; tooltip: "Return to start"; onClicked: root.session.seek(root.session.project.startBar) }
                    Shared.DawButton { objectName: "transport-play"; text: root.session.playing ? "Ⅱ" : "▶"; implicitWidth: 34; accent: true; tooltip: root.session.engineConnected ? "Play engine transport (Space)" : "Play demo transport (Space)"; onClicked: root.session.togglePlayback() }
                    Shared.DawButton { text: "■"; implicitWidth: 27; tooltip: root.session.engineConnected ? "Stop engine transport" : "Stop demo transport"; onClicked: root.session.stop() }
                    Shared.DawButton { text: "●"; implicitWidth: 27; tint: Shared.Theme.danger; enabled: false; tooltip: "Recording requires an audio engine connection" }
                    Rectangle { width: 1; Layout.fillHeight: true; color: Shared.Theme.border; Layout.leftMargin: 5; Layout.rightMargin: 8 }
                    Column {
                        Layout.preferredWidth: 115; spacing: 2
                        Row {
                            spacing: 5
                            Text { text: root.session.engineConnected ? root.session.project.bbtLabel : Math.floor(root.session.playheadBar) + "." + (Math.floor((root.session.playheadBar % 1) * 4) + 1) + "." + (Math.floor((root.session.playheadBar * 4 % 1) * 960) + 1); color: Shared.Theme.ink; font.pixelSize: 18; font.weight: Font.DemiBold; font.family: Shared.Theme.monoFamily }
                            Text {
                                readonly property int seconds: root.session.engineConnected ? Math.floor(root.session.project.elapsedSeconds) : Math.floor((root.session.playheadBar - 1) * 240 / root.session.project.bpm)
                                text: Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
                                color: Shared.Theme.dim; font.pixelSize: 9; anchors.bottom: parent.bottom; anchors.bottomMargin: 3
                            }
                        }
                        Text { text: "BAR · BEAT · TICK"; color: Shared.Theme.faint; font.pixelSize: 8; font.letterSpacing: .7 }
                    }
                    Rectangle { width: 1; Layout.fillHeight: true; color: Shared.Theme.border }
                    Column {
                        Layout.leftMargin: 9; Layout.rightMargin: 8; spacing: 2
                        Text { text: root.session.project.bpm + " BPM"; font.family: Shared.Theme.monoFamily; font.pixelSize: 12; color: Shared.Theme.ink; font.weight: Font.DemiBold }
                        Text { text: root.session.project.timeSig; font.pixelSize: 8; color: Shared.Theme.muted }
                    }
                    Rectangle { width: 1; Layout.fillHeight: true; color: Shared.Theme.border }
                    Column {
                        Layout.leftMargin: 9; Layout.rightMargin: 8; spacing: 2
                        Text { text: root.session.project.key; font.pixelSize: 13; color: Shared.Theme.accent; font.weight: Font.DemiBold }
                        Text { text: "KEY · SCALE"; font.pixelSize: 8; color: Shared.Theme.faint; font.letterSpacing: .6 }
                    }
                    Rectangle { width: 1; Layout.fillHeight: true; color: Shared.Theme.border }
                    Shared.DawButton { text: "⇄"; checked: root.session.loopEnabled; implicitWidth: 27; enabled: !root.session.engineConnected; tooltip: "Loop demo transport"; onClicked: root.session.toggleLoop() }
                    Shared.DawButton { text: "♩"; implicitWidth: 27; tooltip: "Metronome requires an audio engine connection"; enabled: false }
                }
            }
        }
        RowLayout {
            Layout.preferredWidth: root.width < 1300 ? 215 : 300; spacing: 10
            Item { Layout.fillWidth: true }
            Text { text: root.session.engineConnected ? "CPU" : "DEMO"; font.pixelSize: 9; color: Shared.Theme.muted; visible: root.width >= 1300 }
            Rectangle { width: 46; height: 5; radius: 3; color: "#14ffffff"; visible: root.width >= 1300; Rectangle { width: parent.width * (root.session.engineConnected ? Math.max(0, Math.min(1, root.session.project.cpuLoad / 100)) : .31); height: parent.height; radius: 3; color: Shared.Theme.success } }
            Text { text: root.session.engineConnected ? Number(root.session.project.cpuLoad).toFixed(0) + "%" : "31%"; font.pixelSize: 10; font.family: Shared.Theme.monoFamily; color: Shared.Theme.secondary; visible: root.width >= 1300 }
            Shared.StereoMeter { Layout.preferredWidth: 11; Layout.preferredHeight: 22; meterWidth: 4; levelL: root.session.engineConnected ? root.session.project.master.meterL : .62; levelR: root.session.engineConnected ? root.session.project.master.meterR : .52 }
            Shared.DawButton { objectName: "assistant-toggle"; text: "✦ Assistant  ⌘K"; radius: 15; checked: true; implicitWidth: 114; tooltip: "Open command palette (Ctrl+K / Cmd+K)"; onClicked: root.assistantRequested() }
            Row {
                spacing: -8
                Repeater {
                    model: ["JK", "M"]
                    Rectangle {
                        required property string modelData
                        required property int index
                        width: 26; height: 26; radius: 13; border.width: 1.5; border.color: Shared.Theme.rail
                        color: index ? "#d95a3a" : "#3a5bd9"
                        Text { text: parent.modelData; anchors.centerIn: parent; color: "white"; font.pixelSize: 10; font.weight: Font.DemiBold }
                        ToolTip.visible: avatarHover.hovered; ToolTip.text: "Design reference collaborator"
                        HoverHandler { id: avatarHover }
                    }
                }
            }
        }
    }
}
