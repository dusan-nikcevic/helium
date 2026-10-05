pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

ScrollBar {
    id: bar
    policy: ScrollBar.AsNeeded
    padding: 1
    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: Qt.rgba(1, 1, 1, bar.pressed ? 0.28 : 0.16)
        opacity: bar.active ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 160 } }
    }
    background: null
}
