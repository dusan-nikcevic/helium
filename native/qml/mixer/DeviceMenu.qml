pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "../shared" as Shared
import "mixfmt.js" as Fmt

Menu {
    id: menu

    property var session
    property string trackId: ""

    padding: 4
    implicitWidth: 168

    background: Rectangle {
        color: Shared.Theme.elevated
        radius: 8
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.1)
    }

    Instantiator {
        model: ["eq", "dynamics", "saturation", "fx", "instrument", "utility"]
        onObjectAdded: (index, object) => menu.insertItem(index, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
        delegate: MenuItem {
            id: item
            required property string modelData
            text: Fmt.kindLabel(modelData)
            implicitHeight: 26
            onTriggered: menu.session.addDevice(menu.trackId, modelData)
            contentItem: Row {
                spacing: 8
                leftPadding: 4
                Rectangle {
                    width: 8; height: 8; radius: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: Shared.Theme.deviceColor(item.modelData)
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: item.text
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 11
                    color: item.highlighted ? Shared.Theme.ink : Shared.Theme.secondary
                }
            }
            background: Rectangle {
                radius: 5
                color: item.highlighted ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
            }
        }
    }
}
