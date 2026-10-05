pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "../shared" as Shared

Rectangle {
    id: shelf

    property string title: ""
    property string addText: ""
    property bool canAdd: true
    property string addTooltip: ""
    property var model: []
    property Component delegate
    property int slotSpacing: 3
    signal addClicked(Item anchor)

    implicitHeight: 82
    radius: 8
    color: Qt.rgba(0, 0, 0, 0.22)
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.05)
    clip: true

    Item {
        id: head
        x: 7
        width: parent.width - 14
        height: 24

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: shelf.title
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 8
            font.weight: Font.Bold
            font.letterSpacing: 0.6
            color: "#9a9aa0"
        }
        Rectangle {
            id: plus
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 15; height: 15; radius: 4
            color: Qt.rgba(1, 1, 1, plusMouse.containsMouse && shelf.canAdd ? 0.12 : 0.07)
            opacity: shelf.canAdd ? 1 : 0.45
            Text {
                anchors.centerIn: parent
                text: "+"
                font.pixelSize: 12
                color: Shared.Theme.secondary
            }
            MouseArea {
                id: plusMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: shelf.canAdd ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: if (shelf.canAdd) shelf.addClicked(plus)
                ToolTip.visible: !shelf.canAdd && containsMouse && shelf.addTooltip.length > 0
                ToolTip.text: shelf.addTooltip
            }
        }
    }

    ListView {
        id: list
        anchors.top: head.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 7
        anchors.rightMargin: 7
        anchors.bottomMargin: 6
        clip: true
        spacing: shelf.slotSpacing
        boundsBehavior: Flickable.StopAtBounds
        model: shelf.model
        delegate: shelf.delegate
        ScrollBar.vertical: ThinScrollBar { x: list.width - 1 }
        footer: Item {
            width: list.width
            height: 24
            DashedButton {
                y: 3
                width: parent.width
                text: shelf.addText
                opacity: shelf.canAdd ? 1 : 0.45
                onClicked: if (shelf.canAdd) shelf.addClicked(this)
                ToolTip.visible: !shelf.canAdd && hovered && shelf.addTooltip.length > 0
                ToolTip.text: shelf.addTooltip
            }
        }
    }
}
