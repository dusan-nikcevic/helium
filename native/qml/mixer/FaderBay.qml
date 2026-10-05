pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared
import "mixfmt.js" as Fmt

Rectangle {
    id: bay

    property real volumeDb: 0
    property real meterL: 0
    property real meterR: 0
    property int barWidth: 7
    property bool large: false
    property string label: "Channel volume"
    signal volumeEdited(real db)

    readonly property real norm: Fmt.dbToNorm(volumeDb)
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: label
    Accessible.description: Number(volumeDb).toFixed(1) + " dB"
    function adjust(delta) { volumeEdited(Math.max(-60, Math.min(6, volumeDb + delta))) }
    Keys.onUpPressed: adjust(.5)
    Keys.onRightPressed: adjust(.5)
    Keys.onDownPressed: adjust(-.5)
    Keys.onLeftPressed: adjust(-.5)
    border.width: activeFocus ? 1 : 0
    border.color: Shared.Theme.accent

    topLeftRadius: 8
    topRightRadius: 8
    color: Qt.rgba(0, 0, 0, large ? 0.26 : 0.24)

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.bottomMargin: 6
        anchors.leftMargin: bay.large ? 6 : 5
        anchors.rightMargin: bay.large ? 6 : 5
        spacing: bay.large ? 9 : 6

        Item { Layout.fillWidth: true }

        Item {
            id: scaleCol
            Layout.preferredWidth: bay.large ? 13 : 12
            Layout.fillHeight: true
            Repeater {
                model: ["+6", "0", "−6", "−12", "−24", "−∞"]
                Text {
                    required property string modelData
                    required property int index
                    x: scaleCol.width - width
                    y: 3 + index * (scaleCol.height - 6 - height) / 5
                    text: modelData
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 7
                    color: "#525258"
                }
            }
        }

        Item {
            id: track
            Layout.preferredWidth: bay.large ? 10 : 8
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.05)
                gradient: Gradient {
                    GradientStop { position: 0; color: "#070709" }
                    GradientStop { position: 1; color: "#1a1a1d" }
                }
            }
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.height * bay.norm
                radius: bay.large ? 5 : 4
                color: "#2b8fff"
            }
            Rectangle {
                width: bay.large ? 28 : 24
                height: 13
                radius: 3
                x: (parent.width - width) / 2
                y: parent.height * (1 - bay.norm) - height / 2
                border.width: 1
                border.color: Qt.rgba(0, 0, 0, 0.5)
                gradient: Gradient {
                    GradientStop { position: 0; color: "#3c3c42" }
                    GradientStop { position: 1; color: "#202024" }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: bay.large ? 18 : 14
                    height: 1
                    color: Qt.rgba(0, 0, 0, 0.55)
                }
                Rectangle { width: bay.large ? 18 : 14; height: 1; x: (parent.width - width) / 2; y: parent.height / 2 - 3.5; color: Qt.rgba(1, 1, 1, 0.1) }
                Rectangle { width: bay.large ? 18 : 14; height: 1; x: (parent.width - width) / 2; y: parent.height / 2 + 2.5; color: Qt.rgba(1, 1, 1, 0.1) }
            }
            MouseArea {
                objectName: "fader-input"
                anchors.fill: parent
                anchors.leftMargin: -10
                anchors.rightMargin: -10
                anchors.topMargin: -7
                anchors.bottomMargin: -7
                preventStealing: true
                cursorShape: Qt.SizeVerCursor
                function edit(y) { bay.volumeEdited(Fmt.normToDb(1 - (y - 7) / track.height)) }
                onPressed: mouse => { bay.forceActiveFocus(); Session.beginGesture(); edit(mouse.y) }
                onReleased: Session.endGesture()
                onCanceled: Session.endGesture()
                onPositionChanged: mouse => { if (pressed) edit(mouse.y) }
                onDoubleClicked: bay.volumeEdited(0)
                onWheel: wheel => bay.volumeEdited(Math.max(-60, Math.min(6, bay.volumeDb + (wheel.angleDelta.y > 0 ? 0.5 : -0.5))))
            }
        }

        Row {
            Layout.fillHeight: true
            spacing: bay.large ? 3 : 2
            MeterBar { width: bay.barWidth; height: parent.height; level: bay.meterL }
            MeterBar { width: bay.barWidth; height: parent.height; level: bay.meterR }
        }

        Item { Layout.fillWidth: true }
    }
}
