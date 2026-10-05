import QtQuick
import QtQuick.Controls

// why: The default Qt style paints white scrollbar tracks.
ScrollBar {
    id: bar
    property int thickness: 3
    padding: 0
    hoverEnabled: true
    minimumSize: 0.2
    background: Item {}
    contentItem: Rectangle {
        implicitWidth: bar.thickness
        implicitHeight: bar.thickness
        radius: bar.thickness / 2
        color: Qt.rgba(1, 1, 1, bar.pressed ? 0.28 : 0.16)
        opacity: bar.size < 1 && (bar.active || bar.hovered) ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 160 } }
    }
}
