pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import "../shared" as Shared
import "mixfmt.js" as Fmt

Rectangle {
    id: master

    property var session
    property var channel: ({})
    property int barWidth: 7
    property bool showInserts: true
    property bool compact: false
    property real wobbleL: 0
    property real wobbleR: 0
    property bool dim: false
    property bool mono: false
    signal openDevices(string trackId)

    readonly property bool selected: session && session.selectedTrackId === "master"

    function open() {
        session.selectTrack("master")
        openDevices("master")
    }

    radius: 12
    clip: true
    border.width: 1
    border.color: selected ? Qt.rgba(1, 1, 1, 0.4) : Qt.rgba(1, 1, 1, 0.14)
    gradient: Gradient {
        GradientStop { position: 0; color: "#26262a" }
        GradientStop { position: 1; color: "#17171a" }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: master.session.selectTrack("master")
        onDoubleClicked: master.open()
    }

    component Stat: RowLayout {
        property string label
        property string value
        property color valueColor: "#e8e8ea"
        property int valueSize: 11
        property bool mono: false
        Layout.fillWidth: true
        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBaseline
            text: parent.label
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 8
            font.weight: Font.DemiBold
            font.letterSpacing: 0.4
            color: Shared.Theme.dim
        }
        Text {
            Layout.alignment: Qt.AlignBaseline
            text: parent.value
            font.family: parent.mono ? Shared.Theme.monoFamily : Shared.Theme.fontFamily
            font.pixelSize: parent.valueSize
            font.weight: parent.valueSize > 12 ? Font.Bold : Font.DemiBold
            font.features: { "tnum": 1 }
            color: parent.valueColor
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            topLeftRadius: 11
            topRightRadius: 11
            color: Shared.Theme.trackColor("master")
            Text {
                anchors.centerIn: parent
                text: master.channel.name || "Master"
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.Bold
                color: "#16161a"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: master.open()
                }
            }
        }

        Text {
            visible: !master.compact
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "STEREO OUT · MAIN"
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 8
            font.weight: Font.DemiBold
            font.letterSpacing: 0.4
            color: "#7a7a80"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 9
            Layout.rightMargin: 9
            Layout.topMargin: master.compact ? 6 : 0
            Layout.bottomMargin: 8
            Layout.preferredHeight: stats.implicitHeight + 16
            radius: 9
            color: "#0a0a0c"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.08)
            ColumnLayout {
                id: stats
                anchors.fill: parent
                anchors.margins: 8
                anchors.leftMargin: 11
                anchors.rightMargin: 11
                spacing: 6
                Stat {
                    label: "LUFS-I"
                    value: master.session.engineConnected ? "--" : Fmt.signed(master.channel.lufsIntegrated || 0, 1)
                    valueColor: Shared.Theme.accent
                    valueSize: master.compact ? 13 : 16
                    mono: true
                }
                Rectangle {
                    visible: !master.compact
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.rgba(1, 1, 1, 0.06)
                }
                Stat {
                    visible: !master.compact
                    label: "TRUE PK"
                    value: master.session.engineConnected ? "--" : Fmt.signed(master.channel.truePeak || 0, 1) + " dB"
                }
                Stat {
                    visible: !master.compact
                    label: "GR"
                    value: master.session.engineConnected ? "--" : Fmt.signed(master.channel.gainReduction || 0, 1) + " dB"
                    valueColor: Shared.Theme.success
                }
            }
        }

        ColumnLayout {
            visible: master.showInserts
            Layout.fillWidth: true
            Layout.leftMargin: 9
            Layout.rightMargin: 9
            spacing: 3
            Text {
                Layout.bottomMargin: 2
                text: "MASTER CHAIN"
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 8
                font.weight: Font.Bold
                font.letterSpacing: 0.6
                color: "#9a9aa0"
            }
            Repeater {
                model: (master.channel.inserts || []).length
                InsertSlot {
                    required property int index
                    Layout.fillWidth: true
                    session: master.session
                    ownerId: "master"
                    insert: (master.channel.inserts || [])[index] || ({})
                    onOpenRequested: master.open()
                }
            }
        }

        FaderBay {
            label: "Master volume"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.topMargin: 8
            large: true
            barWidth: master.barWidth + 2
            volumeDb: master.channel.volumeDb || 0
            meterL: Math.min(1, (master.channel.meterL || 0) * (1 + master.wobbleL) * (master.dim ? 0.6 : 1))
            meterR: Math.min(1, (master.channel.meterR || 0) * (1 + master.wobbleR) * (master.dim ? 0.6 : 1))
            onVolumeEdited: db => master.session.setTrackValue("master", "volumeDb", db)
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            color: Qt.rgba(0, 0, 0, 0.26)
            Text {
                anchors.centerIn: parent
                text: Fmt.formatDb(master.channel.volumeDb || 0)
                font.family: Shared.Theme.monoFamily
                font.pixelSize: 11
                font.weight: Font.Bold
                color: "#f0f0f2"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.leftMargin: 9
            Layout.rightMargin: 9
            spacing: 4
            Repeater {
                model: ["DIM", "MONO"]
                Rectangle {
                    id: mon
                    opacity: .5
                    Accessible.role: Accessible.StaticText
                    Accessible.description: "Monitoring requires an audio engine connection"
                    required property string modelData
                    readonly property bool active: modelData === "DIM" ? master.dim : master.mono
                    Layout.fillWidth: true
                    Layout.preferredHeight: 20
                    radius: 5
                    color: active ? Qt.rgba(0.91, 0.769, 0.4, 0.9) : Qt.rgba(0, 0, 0, monMouse.containsMouse ? 0.4 : 0.28)
                    Text {
                        anchors.centerIn: parent
                        text: mon.modelData
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        color: mon.active ? "#1a1a1c" : "#9a9aa0"
                    }
                    MouseArea {
                        id: monMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        enabled: false
                        opacity: .5
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            Layout.leftMargin: 9
            Layout.rightMargin: 9
            Layout.bottomMargin: 8
            radius: 6
            color: Qt.rgba(0, 0, 0, 0.26)
            Row {
                anchors.centerIn: parent
                spacing: 5
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7; height: 7; radius: 2
                    color: Shared.Theme.success
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Main Output"
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 9
                    color: "#b6b6bb"
                }
            }
        }
    }
}
