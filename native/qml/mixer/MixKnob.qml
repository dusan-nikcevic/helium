import QtQuick
import QtQuick.Shapes
import Zephyr 1.0
import "../shared" as Shared

Item {
    id: knob

    property real value: 0.5
    property int size: 28
    property color knobColor: Shared.Theme.accent
    property real defaultValue: 0.5
    property string label: "Parameter"
    signal valueEdited(real value)

    readonly property real ring: size > 22 ? 4 : 3
    readonly property real clamped: Math.max(0, Math.min(1, value))

    implicitWidth: size
    implicitHeight: size
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: label
    Accessible.description: Math.round(clamped * 100) + "%"
    function adjust(delta) { valueEdited(Math.max(0, Math.min(1, clamped + delta))) }
    Keys.onUpPressed: adjust(.01)
    Keys.onRightPressed: adjust(.01)
    Keys.onDownPressed: adjust(-.01)
    Keys.onLeftPressed: adjust(-.01)
    Rectangle { anchors.fill: parent; radius: width / 2; color: "transparent"; border.color: Shared.Theme.accent; visible: knob.activeFocus }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.rgba(1, 1, 1, 0.08)
            strokeWidth: knob.ring
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            PathAngleArc {
                centerX: knob.size / 2; centerY: knob.size / 2
                radiusX: (knob.size - knob.ring) / 2; radiusY: radiusX
                startAngle: 135; sweepAngle: 270
            }
        }
        ShapePath {
            strokeColor: knob.knobColor
            strokeWidth: knob.ring
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            PathAngleArc {
                centerX: knob.size / 2; centerY: knob.size / 2
                radiusX: (knob.size - knob.ring) / 2; radiusY: radiusX
                startAngle: 135; sweepAngle: 270 * knob.clamped
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: knob.ring
        radius: width / 2
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.06)
        gradient: Gradient {
            GradientStop { position: 0; color: "#34343a" }
            GradientStop { position: 1; color: "#131315" }
        }
    }

    Rectangle {
        width: 2
        height: knob.size * 0.34
        radius: 1
        color: "#e8e8ea"
        x: knob.size / 2 - 1
        y: knob.size / 2 - height
        transformOrigin: Item.Bottom
        rotation: -135 + knob.clamped * 270
        antialiasing: true
    }

    MouseArea {
        anchors.fill: parent
        preventStealing: true
        cursorShape: Qt.SizeVerCursor
        property real startY
        property real startValue
        onPressed: mouse => { knob.forceActiveFocus(); Session.beginGesture(); startY = mouse.y; startValue = knob.clamped }
        onReleased: Session.endGesture()
        onCanceled: Session.endGesture()
        onPositionChanged: mouse => {
            if (pressed)
                knob.valueEdited(Math.max(0, Math.min(1, startValue + (startY - mouse.y) / 160)))
        }
        onDoubleClicked: knob.valueEdited(knob.defaultValue)
        onWheel: wheel => knob.valueEdited(Math.max(0, Math.min(1, knob.clamped + (wheel.angleDelta.y > 0 ? 0.02 : -0.02))))
    }
}
