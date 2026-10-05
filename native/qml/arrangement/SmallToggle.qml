pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "../shared" as Shared

Button {
    id: chip
    property bool on: false
    property color onColor: "#c0563f"
    property color onTextColor: "#ffffff"

    implicitWidth: 17
    implicitHeight: 15
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.checkable: true
    Accessible.checked: on

    background: Rectangle {
        radius: 4
        color: chip.on ? chip.onColor : Qt.rgba(1, 1, 1, chip.hovered ? 0.1 : 0.06)
        border.width: chip.visualFocus ? 1 : 0
        border.color: Shared.Theme.accent
    }
    contentItem: Text {
        text: chip.text
        font.family: Shared.Theme.fontFamily
        font.pixelSize: 9
        font.weight: chip.on ? Font.Bold : Font.DemiBold
        color: chip.on ? chip.onTextColor : Shared.Theme.muted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
