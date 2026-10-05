pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../shared" as Shared

Rectangle {
    id: root
    property var session
    readonly property var history: session ? session.mixHistory : []
    readonly property var snapshots: session ? session.mixSnapshots : []
    property string expandedId: history.length ? history[0].id : ""
    color: Shared.Theme.rail

    function timeLabel(timestamp) {
        return Qt.formatDateTime(new Date(timestamp), "hh:mm:ss")
    }

    component Label: Text {
        font.family: Shared.Theme.fontFamily
        font.pixelSize: 11
        color: Shared.Theme.secondary
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }

    Rectangle { width: parent.width; height: 1; color: Shared.Theme.border }
    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Mix history"; color: Shared.Theme.ink; font.weight: Font.DemiBold }
                Item { Layout.fillWidth: true }
                Shared.DawButton {
                    objectName: "mixHistoryUndo"
                    text: "Undo latest"
                    enabled: root.session && !root.session.commandBusy && root.session.canUndo
                    tooltip: "Undo the latest session edit"
                    onClicked: root.session.undo()
                }
            }
            ListView {
                id: historyList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: root.history
                clip: true
                spacing: 4
                ScrollBar.vertical: ScrollBar {}
                delegate: ColumnLayout {
                    id: transaction
                    required property var modelData
                    width: historyList.width - 12
                    spacing: 4
                    readonly property bool expanded: root.expandedId === modelData.id
                    Shared.DawButton {
                        Layout.fillWidth: true
                        text: (transaction.expanded ? "▾ " : "▸ ") + transaction.modelData.label
                            + " · " + transaction.modelData.changes.length + " settings"
                            + (transaction.modelData.undone ? " · Undone" : "")
                        checked: transaction.expanded
                        tooltip: "Inspect " + transaction.modelData.label
                        onClicked: root.expandedId = transaction.expanded ? "" : transaction.modelData.id
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        text: (transaction.modelData.source === "ai" ? "Assistant" : transaction.modelData.source === "snapshot" ? "Snapshot" : "Manual")
                            + " · " + root.timeLabel(transaction.modelData.timestamp)
                        color: transaction.modelData.undone ? Shared.Theme.muted : Shared.Theme.secondary
                    }
                    Repeater {
                        model: transaction.expanded ? transaction.modelData.changes : []
                        Label {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.leftMargin: 8
                            wrapMode: Text.Wrap
                            elide: Text.ElideNone
                            text: modelData.channelName + " / " + modelData.label + "  "
                                + modelData.beforeDisplay + " → " + modelData.afterDisplay
                            color: transaction.modelData.undone ? Shared.Theme.muted : Shared.Theme.secondary
                        }
                    }
                }
                Label {
                    anchors.fill: parent
                    visible: historyList.count === 0
                    wrapMode: Text.Wrap
                    text: "Your mixer edits appear here with before and after values. Assistant edits form one undo step."
                    color: Shared.Theme.muted
                }
            }
        }

        Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Shared.Theme.border }

        ColumnLayout {
            Layout.preferredWidth: Math.max(220, Math.min(330, root.width * 0.35))
            Layout.fillHeight: true
            spacing: 8
            Label { text: "Mix snapshots"; color: Shared.Theme.ink; font.weight: Font.DemiBold }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                TextField {
                    id: snapshotName
                    objectName: "mixSnapshotName"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    maximumLength: 80
                    placeholderText: "Name this mix"
                    Accessible.name: "Mix snapshot name"
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 11
                    color: Shared.Theme.ink
                    placeholderTextColor: Shared.Theme.muted
                    selectionColor: Shared.Theme.alpha(Shared.Theme.accent, 0.4)
                    background: Rectangle {
                        radius: 7
                        color: Shared.Theme.well
                        border.color: snapshotName.activeFocus ? Shared.Theme.accent : Shared.Theme.border
                    }
                    onAccepted: saveSnapshot.clicked()
                }
                Shared.DawButton {
                    id: saveSnapshot
                    objectName: "mixSnapshotSave"
                    text: "Save"
                    enabled: snapshotName.text.trim().length > 0
                    tooltip: "Save gain, pan, switches, processors, sends and routing"
                    onClicked: {
                        if (root.session.captureMixSnapshot(snapshotName.text)) snapshotName.clear()
                    }
                }
            }
            ListView {
                id: snapshotList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: root.snapshots
                clip: true
                spacing: 8
                ScrollBar.vertical: ScrollBar {}
                delegate: ColumnLayout {
                    id: savedMix
                    required property var modelData
                    width: snapshotList.width - 12
                    spacing: 4
                    Label {
                        Layout.fillWidth: true
                        text: savedMix.modelData.name
                        color: Shared.Theme.ink
                        font.weight: Font.DemiBold
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: savedMix.modelData.channelCount + " channels · " + root.timeLabel(savedMix.modelData.timestamp)
                            color: Shared.Theme.muted
                        }
                        Shared.DawButton {
                            text: "Restore"
                            objectName: "restore-snapshot-" + savedMix.modelData.id
                            tooltip: "Restore " + savedMix.modelData.name + " as one undo step"
                            onClicked: root.session.restoreMixSnapshot(savedMix.modelData.id)
                        }
                        Shared.DawButton {
                            text: "×"
                            tooltip: "Delete snapshot " + savedMix.modelData.name
                            onClicked: root.session.removeMixSnapshot(savedMix.modelData.id)
                        }
                    }
                }
                Label {
                    anchors.fill: parent
                    visible: snapshotList.count === 0
                    wrapMode: Text.Wrap
                    text: "Save a mix before trying another balance. Restoring a snapshot keeps clips and selection."
                    color: Shared.Theme.muted
                }
            }
        }
    }
}
