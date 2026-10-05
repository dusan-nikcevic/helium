pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../shared" as Shared
import "mixfmt.js" as Fmt

Rectangle {
    id: card

    property var session
    property string trackId: ""
    property var device: ({})

    readonly property color deviceColor: Shared.Theme.deviceColor(device.kind)
    readonly property bool on: device.enabled !== false
    readonly property bool ai: !!device.aiEdited
    readonly property var params: device.params || []
    readonly property string graphKind: device.graph || (device.kind === "eq" ? "eq" : "")

    width: 160
    radius: 10
    clip: true
    color: Shared.Theme.strip
    border.width: 1
    border.color: ai ? Qt.rgba(0.161, 0.592, 1, 0.45) : Qt.rgba(1, 1, 1, 0.08)
    opacity: on ? 1 : 0.55

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            topLeftRadius: 9
            topRightRadius: 9
            color: card.deviceColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                Rectangle {
                    id: led
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: card.on ? "#1c3a24" : "#5a5a5f"
                    border.width: 1
                    border.color: Qt.rgba(0, 0, 0, 0.25)
                    Rectangle {
                        visible: card.on
                        anchors.centerIn: parent
                        width: 11; height: 11; radius: 5.5
                        color: Qt.rgba(0.157, 0.784, 0.431, 0.35)
                        z: -1
                    }
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: card.session.setDeviceEnabled(card.trackId, card.device.id, !card.on)
                        ToolTip.visible: containsMouse
                        ToolTip.delay: 600
                        ToolTip.text: card.on ? "Bypass device" : "Enable device"
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: card.device.name || ""
                    elide: Text.ElideRight
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.1
                    color: "#16161a"
                }
                Text {
                    visible: card.ai
                    text: "✦"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    color: "#0a4da8"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 11
            Layout.rightMargin: 11
            Layout.topMargin: 5
            Layout.bottomMargin: 3
            Text {
                Layout.fillWidth: true
                text: Fmt.kindLabel(card.device.kind).toUpperCase()
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 8
                font.weight: Font.DemiBold
                font.letterSpacing: 0.3
                color: "#7a7a80"
            }
            Text {
                text: card.device.preset || "Custom"
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 8
                color: "#5a5a5f"
            }
        }

        Rectangle {
            visible: card.graphKind.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            Layout.leftMargin: 11
            Layout.rightMargin: 11
            Layout.topMargin: 2
            Layout.bottomMargin: 4
            radius: 6
            color: Shared.Theme.well
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.06)
            clip: true

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: Qt.rgba(1, 1, 1, 0.08)
                    strokeWidth: 1
                    strokeStyle: card.graphKind === "comp" ? ShapePath.DashLine : ShapePath.SolidLine
                    dashPattern: [2, 3]
                    fillColor: "transparent"
                    PathSvg {
                        path: card.graphKind === "comp" ? "M 3 " + (34 - 3) + " L " + (136 - 3) + " 3"
                                                        : "M 0 17 L 136 17"
                    }
                }
                ShapePath {
                    strokeColor: card.deviceColor
                    strokeWidth: 1.6
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg {
                        path: card.graphKind === "comp" ? Fmt.compPath(card.params, 136, 34) : Fmt.eqPath(card.params, 136, 34)
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 11
            Layout.rightMargin: 11
            Layout.topMargin: 6
            Layout.bottomMargin: 11
            spacing: 10

            Repeater {
                model: card.params.length
                Column {
                    id: ctl
                    required property int index
                    readonly property var modelData: card.params[index] || ({ value: 0, label: "" })
                    width: 38
                    spacing: 4
                    MixKnob {
                        id: paramKnob
                        objectName: "device-knob-" + card.device.id + "-" + ctl.modelData.id
                        anchors.horizontalCenter: parent.horizontalCenter
                        size: 26
                        label: card.device.name + " " + ctl.modelData.label
                        value: ctl.modelData.value
                        knobColor: ctl.modelData.aiEdited ? Shared.Theme.accent : card.deviceColor
                        onValueEdited: v => card.session.setDeviceParam(card.trackId, card.device.id, ctl.modelData.id, v)
                        HoverHandler { id: knobHover }
                        ToolTip.visible: knobHover.hovered && !!ctl.modelData.display
                        ToolTip.delay: 300
                        ToolTip.text: ctl.modelData.label + " " + (ctl.modelData.display || "")
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: ctl.modelData.label
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 8
                        font.weight: Font.Medium
                        color: ctl.modelData.aiEdited ? "#9cc7ff" : "#9a9aa0"
                    }
                }
            }
        }
    }
}
