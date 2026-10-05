pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import "../shared" as Shared
import "mixfmt.js" as Fmt

Rectangle {
    id: strip

    property var session
    property var channel: ({})
    property bool isBus: false
    property color channelColor: Shared.Theme.trackColor(channel.color)
    property int memberCount: 0
    property string outName: "Master"
    property color outColor: Shared.Theme.trackColor("master")
    property int barWidth: 7
    property bool showInserts: true
    property bool showSends: true
    property bool compact: false
    property real wobbleL: 0
    property real wobbleR: 0
    signal openDevices(string trackId)

    readonly property string channelId: channel.id || ""
    readonly property bool ai: !!channel.aiEdited
    readonly property bool selected: session && session.selectedTrackId === channelId

    function open() {
        session.selectTrack(channelId)
        openDevices(channelId)
    }

    radius: 11
    clip: true
    border.width: 1
    border.color: selected ? Qt.rgba(1, 1, 1, 0.55)
                : ai ? Qt.rgba(0.161, 0.592, 1, 0.45)
                : Shared.Theme.alpha(channelColor, isBus ? 0.5 : 0.3)
    gradient: Gradient {
        GradientStop { position: 0; color: Shared.Theme.alpha(strip.channelColor, strip.isBus ? 0.4 : 0.3) }
        GradientStop { position: strip.isBus ? 0.3 : 0.24; color: Shared.Theme.alpha(strip.channelColor, 0.11) }
        GradientStop { position: strip.isBus ? 0.56 : 0.52; color: "#18181a" }
        GradientStop { position: 1; color: "#18181a" }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: strip.session.selectTrack(strip.channelId)
        onDoubleClicked: strip.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            topLeftRadius: 10
            topRightRadius: 10
            color: strip.channelColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                spacing: 5

                Shape {
                    visible: strip.isBus
                    Layout.preferredWidth: 11
                    Layout.preferredHeight: 11
                    scale: 11 / 24
                    transformOrigin: Item.TopLeft
                    ShapePath {
                        fillColor: "#16161a"
                        strokeColor: "transparent"
                        PathSvg { path: "M3 6.5A1.5 1.5 0 0 1 4.5 5h4l2 2.2H19.5A1.5 1.5 0 0 1 21 8.7v9.8A1.5 1.5 0 0 1 19.5 20h-15A1.5 1.5 0 0 1 3 18.5z" }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: strip.channel.name || ""
                    elide: Text.ElideRight
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: "#16161a"
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: strip.open()
                    }
                }
                Rectangle {
                    visible: strip.isBus
                    Layout.preferredHeight: 13
                    Layout.preferredWidth: sumText.implicitWidth + 12
                    radius: 4
                    color: Qt.rgba(0, 0, 0, 0.16)
                    Text {
                        id: sumText
                        anchors.centerIn: parent
                        text: "∑ " + strip.memberCount
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        color: "#16161a"
                    }
                }
                Text {
                    visible: strip.ai
                    text: "✦"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    color: "#0a4da8"
                }
            }
        }

        Text {
            visible: !strip.compact
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: strip.isBus ? "GROUP OUT → MASTER" : Fmt.trackTypeLabel(strip.channel.kind).toUpperCase()
            elide: Text.ElideRight
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 8
            font.weight: Font.DemiBold
            font.letterSpacing: 0.4
            color: "#7a7a80"
        }

        Shelf {
            visible: strip.showInserts
            Layout.fillWidth: true
            Layout.preferredHeight: 82
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.topMargin: 3
            title: "INSERTS"
            addText: "+ Insert"
            model: (strip.channel.inserts || []).length
            onAddClicked: anchor => {
                strip.session.selectTrack(strip.channelId)
                addMenu.popup(anchor, 0, anchor.height)
            }
            delegate: InsertSlot {
                required property int index
                width: ListView.view.width
                session: strip.session
                ownerId: strip.channelId
                insert: (strip.channel.inserts || [])[index] || ({})
                onOpenRequested: strip.open()
            }
        }

        Shelf {
            visible: strip.showSends
            Layout.fillWidth: true
            Layout.preferredHeight: 82
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.topMargin: 5
            title: "SENDS"
            addText: "+ Send"
            canAdd: false
            addTooltip: "Send routing is not exposed by the session yet"
            slotSpacing: 6
            model: (strip.channel.sends || []).length
            delegate: RowLayout {
                id: sendRow
                required property int index
                readonly property var modelData: (strip.channel.sends || [])[index] || ({ amount: 0, name: "" })
                width: ListView.view.width
                height: 16
                spacing: 7
                MixKnob {
                    size: 16
                    label: strip.channel.name + " send to " + sendRow.modelData.name
                    value: sendRow.modelData.amount / 100
                    defaultValue: 0
                    knobColor: sendRow.modelData.aiEdited ? Shared.Theme.accent : "#7a7a82"
                    onValueEdited: v => strip.session.setSendAmount(strip.channelId, sendRow.modelData.id, Math.round(v * 100))
                }
                Text {
                    Layout.fillWidth: true
                    text: sendRow.modelData.name
                    elide: Text.ElideRight
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 9
                    color: sendRow.modelData.aiEdited ? "#9cc7ff" : "#b2b2b8"
                }
                Text {
                    text: Math.round(sendRow.modelData.amount) + "%"
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 9
                    font.features: { "tnum": 1 }
                    color: "#9a9aa0"
                }
            }
        }

        Item {
            visible: !strip.compact
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                spacing: 3
                MixKnob {
                    anchors.horizontalCenter: parent.horizontalCenter
                    label: strip.channel.name + " pan"
                    size: 28
                    enabled: !strip.session.engineConnected || !!strip.channel.panAvailable
                    value: ((strip.channel.pan || 0) + 1) / 2
                    onValueEdited: v => strip.session.setTrackValue(strip.channelId, "pan", Math.round((v * 2 - 1) * 100) / 100)
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    textFormat: Text.StyledText
                    text: "PAN <font color=\"#c5c5ca\">" + Fmt.panLabel(strip.channel.pan || 0) + "</font>"
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.4
                    color: Shared.Theme.muted
                }
            }
        }

        FaderBay {
            objectName: "fader-" + strip.channelId
            label: strip.channel.name + " volume"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            Layout.topMargin: 4
            barWidth: strip.barWidth
            volumeDb: strip.channel.volumeDb !== undefined ? strip.channel.volumeDb : 0
            meterL: strip.channel.muted ? 0 : Math.min(1, (strip.channel.meterL || 0) * (1 + strip.wobbleL))
            meterR: strip.channel.muted ? 0 : Math.min(1, (strip.channel.meterR || 0) * (1 + strip.wobbleR))
            onVolumeEdited: db => strip.session.setTrackValue(strip.channelId, "volumeDb", db)
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            color: Qt.rgba(0, 0, 0, 0.24)
            Text {
                anchors.centerIn: parent
                text: Fmt.formatDb(strip.channel.volumeDb || 0)
                font.family: Shared.Theme.monoFamily
                font.pixelSize: 10
                font.weight: Font.DemiBold
                color: "#e2e2e6"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            spacing: 3
            Repeater {
                model: [
                    { label: "M", key: "muted", on: "#c0563f", ink: "#ffffff" },
                    { label: "S", key: "soloed", on: "#e8c466", ink: "#1a1a1c" },
                    { label: "R", key: "recordArmed", on: Shared.Theme.danger, ink: "#ffffff" }
                ]
                Rectangle {
                    id: msr
                    required property var modelData
                    objectName: "switch-" + strip.channelId + "-" + modelData.key
                    readonly property bool active: !!strip.channel[modelData.key]
                    readonly property bool usable: !((strip.isBus || strip.session.engineConnected) && modelData.key === "recordArmed")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 20
                    radius: 4
                    color: active ? modelData.on : Qt.rgba(0, 0, 0, msrMouse.containsMouse && usable ? 0.4 : 0.28)
                    opacity: usable ? 1 : 0.5
                    Text {
                        anchors.centerIn: parent
                        text: msr.modelData.label
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        color: msr.active ? msr.modelData.ink : "#9a9aa0"
                    }
                    MouseArea {
                        id: msrMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: msr.usable
                        cursorShape: Qt.PointingHandCursor
                        onClicked: strip.session.setTrackValue(strip.channelId, msr.modelData.key, !msr.active)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.topMargin: 4
            Layout.bottomMargin: 8
            radius: 6
            color: Qt.rgba(0, 0, 0, 0.26)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.05)
            Row {
                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.width - 14)
                spacing: 5
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7; height: 7; radius: 2
                    color: strip.outColor
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, parent.parent.width - 26)
                    text: strip.outName
                    elide: Text.ElideRight
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 9
                    color: "#d2d2d6"
                }
            }
        }
    }

    DeviceMenu {
        id: addMenu
        session: strip.session
        trackId: strip.channelId
    }
}
