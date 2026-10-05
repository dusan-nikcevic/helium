import QtQuick
import QtQuick.Shapes
import Zephyr 1.0

Item {
    id: root
    property real value: .5
    property int size: 28
    property color knobColor: Theme.accent
    property string label: ""
    property string display: ""
    signal valueEdited(real value)
    implicitWidth: Math.max(size, labelText.implicitWidth, displayText.implicitWidth)
    implicitHeight: size + (label ? 14 : 0) + (display ? 12 : 0)
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: label || "Knob"
    Accessible.description: display || Math.round(value * 100) + "%"
    function adjust(delta: real) { valueEdited(Math.max(0, Math.min(1, value + delta))) }
    Keys.onLeftPressed: adjust(-.01)
    Keys.onRightPressed: adjust(.01)
    Keys.onUpPressed: adjust(.01)
    Keys.onDownPressed: adjust(-.01)
    Item {
        width: root.size; height: root.size
        anchors.horizontalCenter: parent.horizontalCenter
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeWidth: 2.5; strokeColor: "#20ffffff"; fillColor: "transparent"
                PathAngleArc { centerX: root.size / 2; centerY: root.size / 2; radiusX: root.size / 2 - 2; radiusY: radiusX; startAngle: 135; sweepAngle: 270 }
            }
            ShapePath {
                strokeWidth: 2.5; strokeColor: root.knobColor; fillColor: "transparent"
                PathAngleArc { centerX: root.size / 2; centerY: root.size / 2; radiusX: root.size / 2 - 2; radiusY: radiusX; startAngle: 135; sweepAngle: Math.max(0, Math.min(1, root.value)) * 270 }
            }
        }
        Rectangle {
            anchors.fill: parent; anchors.margins: 4
            radius: width / 2
            gradient: Gradient { GradientStop { position: 0; color: "#34343a" } GradientStop { position: 1; color: "#131315" } }
            border.color: root.activeFocus ? Theme.accent : "#20ffffff"
        }
        Item {
            anchors.fill: parent
            rotation: -135 + root.value * 270
            Rectangle { x: parent.width / 2 - 1; y: 5; width: 2; height: root.size * .28; radius: 1; color: Theme.ink }
        }
        MouseArea {
            anchors.fill: parent
            property real startY
            property real startValue
            cursorShape: Qt.SizeVerCursor
            onPressed: mouse => { root.forceActiveFocus(); Session.beginGesture(); startY = mouse.y; startValue = root.value }
            onReleased: Session.endGesture()
            onCanceled: Session.endGesture()
            onPositionChanged: mouse => { if (pressed) root.valueEdited(Math.max(0, Math.min(1, startValue + (startY - mouse.y) / 140))) }
            onWheel: wheel => { root.adjust(wheel.angleDelta.y > 0 ? .02 : -.02); wheel.accepted = true }
            onDoubleClicked: root.valueEdited(.5)
        }
    }
    Text { id: labelText; anchors.top: parent.top; anchors.topMargin: root.size + 3; anchors.horizontalCenter: parent.horizontalCenter; text: root.label; color: Theme.muted; font.family: Theme.fontFamily; font.pixelSize: 8 }
    Text { id: displayText; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; text: root.display; color: Theme.secondary; font.family: Theme.monoFamily; font.pixelSize: 8; visible: root.display.length > 0 }
}
