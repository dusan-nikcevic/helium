import QtQuick
import QtQuick.Shapes
import "../shared" as Shared

Item {
    id: btn

    property string text: ""
    property real radius: 5
    property int fontSize: 9
    property bool hovered: mouse.containsMouse
    default property alias content: holder.data
    signal clicked()

    implicitHeight: 21

    Shape {
        anchors.fill: parent
        ShapePath {
            strokeColor: Qt.rgba(1, 1, 1, btn.hovered ? 0.26 : 0.16)
            strokeWidth: 1
            strokeStyle: ShapePath.DashLine
            dashPattern: [3, 2]
            fillColor: btn.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
            PathRectangle {
                x: 0.5; y: 0.5
                width: btn.width - 1; height: btn.height - 1
                radius: btn.radius
            }
        }
    }

    Text {
        visible: btn.text.length > 0
        anchors.centerIn: parent
        text: btn.text
        font.family: Shared.Theme.fontFamily
        font.pixelSize: btn.fontSize
        font.weight: Font.Medium
        color: btn.hovered ? Shared.Theme.secondary : Shared.Theme.muted
    }

    Item { id: holder; anchors.fill: parent }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: btn.clicked()
    }
}
