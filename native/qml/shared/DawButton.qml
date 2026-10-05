import QtQuick
import QtQuick.Controls

Button {
    id: root
    property bool accent: false
    property bool compact: true
    property color tint: Theme.accent
    property string tooltip: ""
    property int radius: 7
    implicitHeight: compact ? 26 : 32
    implicitWidth: Math.max(26, label.implicitWidth + 20)
    padding: 0
    hoverEnabled: true
    Accessible.name: tooltip || text
    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.text: tooltip
    ToolTip.delay: 600
    background: Rectangle {
        radius: root.radius
        color: root.down ? Qt.darker(root.tint, 1.4) : root.accent ? root.tint
            : root.checked ? Theme.alpha(root.tint, .14) : root.hovered ? "#16ffffff" : "transparent"
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.accent : root.checked ? Theme.alpha(root.tint, .32) : Theme.border
    }
    contentItem: Text {
        id: label
        text: root.text
        font.family: Theme.fontFamily
        font.pixelSize: 11
        font.weight: root.checked || root.accent ? Font.DemiBold : Font.Normal
        color: !root.enabled ? Theme.faint : root.accent ? "white" : root.checked ? root.tint : Theme.secondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
