import QtQuick
import QtQuick.Controls
import "../shared" as Shared

Rectangle {
    id: slot

    property var session
    property string ownerId: ""
    property var insert: ({})
    signal openRequested()

    readonly property bool ai: !!insert.aiEdited
    readonly property bool on: insert.enabled !== false

    implicitHeight: 19
    radius: 5
    color: ai ? Qt.rgba(0.161, 0.592, 1, 0.14) : Qt.rgba(1, 1, 1, nameMouse.containsMouse ? 0.08 : 0.05)
    border.width: 1
    border.color: ai ? Qt.rgba(0.161, 0.592, 1, 0.36) : Qt.rgba(1, 1, 1, 0.06)

    Rectangle {
        id: led
        x: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 5; height: 5; radius: 2.5
        color: !slot.on ? Shared.Theme.faint : slot.ai ? Shared.Theme.accent : Shared.Theme.success
        Rectangle {
            visible: slot.on
            anchors.centerIn: parent
            width: 9; height: 9; radius: 4.5
            color: Shared.Theme.alpha(led.color, 0.25)
            z: -1
        }
        MouseArea {
            anchors.fill: parent
            anchors.margins: -5
            cursorShape: Qt.PointingHandCursor
            onClicked: slot.session.setInsertEnabled(slot.ownerId, slot.insert.id, !slot.on)
            ToolTip.visible: containsMouse
            ToolTip.text: slot.on ? "Bypass " + slot.insert.name : "Enable " + slot.insert.name
            ToolTip.delay: 600
            hoverEnabled: true
        }
    }

    Text {
        anchors.left: led.right
        anchors.leftMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: slot.insert.name || ""
        elide: Text.ElideRight
        font.family: Shared.Theme.fontFamily
        font.pixelSize: 10
        font.strikeout: !slot.on
        color: !slot.on ? Shared.Theme.dim : slot.ai ? "#9cc7ff" : "#c2c2c8"
        MouseArea {
            id: nameMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: slot.openRequested()
        }
    }
}
