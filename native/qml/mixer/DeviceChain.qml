pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root

    property var session: Session
    property var track: session.selectedTrack
    signal closeRequested()

    readonly property var project: session ? session.project : null
    readonly property var devices: track && track.devices ? track.devices : []
    readonly property color trackColor: track ? Shared.Theme.trackColor(track.color || "master") : Shared.Theme.muted
    readonly property string trackId: track ? (track.id || "master") : ""
    readonly property string outName: {
        if (!track) return "OUT"
        if (trackId === "master") return "Main"
        if (track.busId && project) {
            const bus = project.buses.find(b => b.id === track.busId)
            if (bus) return bus.name
        }
        return track.output || "Master"
    }

    color: "#141416"
    clip: true

    Rectangle {
        width: parent.width
        height: 1
        z: 1
        color: Qt.rgba(1, 1, 1, 0.09)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: "#101012"
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 9

                Rectangle {
                    Layout.preferredWidth: 10
                    Layout.preferredHeight: 10
                    radius: 3
                    color: root.trackColor
                }
                Text {
                    text: root.track ? root.track.name : "No track selected"
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.2
                    color: Shared.Theme.ink
                }
                Text {
                    text: "DEVICE CHAIN"
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.4
                    color: Shared.Theme.dim
                }
                Text {
                    visible: root.track !== null && root.track !== undefined
                    text: root.devices.length + (root.devices.length === 1 ? " device" : " devices")
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 11
                    color: Shared.Theme.muted
                }
                Item { Layout.fillWidth: true }
                Row {
                    spacing: 5
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 6; height: 6; radius: 3
                        color: Shared.Theme.success
                    }
                    Text {
                        text: "Signal flow →"
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 11
                        color: Shared.Theme.muted
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    radius: 6
                    color: Qt.rgba(1, 1, 1, closeMouse.containsMouse ? 0.12 : 0.06)
                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: 14
                        color: closeMouse.containsMouse ? Shared.Theme.ink : "#b6b6bb"
                    }
                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.closeRequested()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            gradient: Gradient {
                GradientStop { position: 0; color: "#161618" }
                GradientStop { position: 1; color: "#1b1b1e" }
            }

            Text {
                visible: !root.track
                anchors.centerIn: parent
                text: "Select a track to see its device chain."
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 12
                color: Shared.Theme.muted
            }

            Flickable {
                id: scroller
                visible: !!root.track
                anchors.fill: parent
                contentWidth: chain.width + 28
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.horizontal: ThinScrollBar { thickness: 6; y: scroller.height - 8 }

                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    orientation: Qt.Vertical
                    onWheel: event => {
                        const max = Math.max(0, scroller.contentWidth - scroller.width)
                        scroller.contentX = Math.max(0, Math.min(max, scroller.contentX - event.angleDelta.y))
                        event.accepted = true
                    }
                }

                Row {
                    id: chain
                    x: 14
                    y: 12
                    height: scroller.height - 24
                    spacing: 10

                    component FlowChip: Column {
                        id: chip
                        property string label
                        property color fill: "transparent"
                        property color edge: Qt.rgba(1, 1, 1, 0.1)
                        property string glyph: ""
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 30; height: 30; radius: 8
                            color: chip.fill
                            border.width: 1
                            border.color: chip.edge
                            Text {
                                anchors.centerIn: parent
                                text: chip.glyph
                                font.pixelSize: 13
                                color: Shared.Theme.dim
                            }
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: chip.label
                            font.family: Shared.Theme.fontFamily
                            font.pixelSize: 8
                            font.letterSpacing: 0.4
                            color: Shared.Theme.dim
                        }
                    }

                    component Arrow: Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "→"
                        font.pixelSize: 13
                        color: "#4a4a4f"
                    }

                    FlowChip { label: "IN"; glyph: "⌁" }

                    Repeater {
                        model: root.devices.length
                        Row {
                            id: link
                            required property int index
                            readonly property var modelData: root.devices[index] || ({})
                            height: chain.height
                            spacing: 10
                            DeviceCard {
                                height: parent.height
                                session: root.session
                                trackId: root.trackId
                                device: link.modelData
                            }
                            Arrow {}
                        }
                    }

                    DashedButton {
                        width: root.devices.length === 0 ? 220 : 120
                        height: chain.height
                        radius: 11
                        onClicked: addMenu.popup(this, 0, height)
                        Column {
                            anchors.centerIn: parent
                            spacing: 7
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "+"
                                font.pixelSize: 20
                                color: Shared.Theme.dim
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Drop a device"
                                font.family: Shared.Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: Shared.Theme.dim
                            }
                            Text {
                                visible: root.devices.length === 0
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "No devices on this channel yet"
                                font.family: Shared.Theme.fontFamily
                                font.pixelSize: 9
                                color: Shared.Theme.faint
                            }
                        }
                    }

                    FlowChip {
                        label: root.outName.toUpperCase()
                        fill: Shared.Theme.alpha(root.trackColor, 0.16)
                        edge: Shared.Theme.alpha(root.trackColor, 0.5)
                    }
                }
            }
        }
    }

    DeviceMenu {
        id: addMenu
        session: root.session
        trackId: root.trackId
    }
}
