pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import "../shared" as Shared

Rectangle {
    id: panel

    property var mixer
    property var group: ({})
    property int groupIndex: 0

    readonly property color groupColor: Shared.Theme.trackColor(group.color)
    readonly property var bus: mixer.busById(group.busId)
    readonly property bool expanded: !mixer.collapsed[group.id]
    readonly property var members: (group.trackIds || []).map(id => mixer.trackById(id)).filter(t => t !== null)
    readonly property var shownMembers: members.filter(t => mixer.matches(t.name, group))
    readonly property bool busShown: bus !== null && (mixer.matches(bus.name, group) || shownMembers.length > 0)

    visible: busShown || shownMembers.length > 0
    width: visible ? content.implicitWidth + 16 : 0
    radius: 13
    color: Shared.Theme.alpha(groupColor, 0.06)
    border.width: 1
    border.color: Shared.Theme.alpha(groupColor, 0.18)

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.minimumWidth: header.implicitWidth + 18
            radius: 8
            color: Shared.Theme.alpha(panel.groupColor, 0.16)
            border.width: 1
            border.color: Shared.Theme.alpha(panel.groupColor, 0.28)

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: panel.mixer.toggleGroup(panel.group.id)
            }

            RowLayout {
                id: header
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                spacing: 7

                Shape {
                    Layout.preferredWidth: 10
                    Layout.preferredHeight: 10
                    rotation: panel.expanded ? 90 : 0
                    ShapePath {
                        fillColor: Shared.Theme.alpha(panel.groupColor, 0.95)
                        strokeColor: "transparent"
                        PathSvg { path: "M 3 1.5 L 8 5 L 3 8.5 Z" }
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 2
                    color: panel.groupColor
                }
                Text {
                    text: panel.group.name || ""
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.1
                    color: "#f0f0f2"
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    visible: panel.expanded && panel.bus !== null
                    Layout.preferredHeight: 15
                    Layout.preferredWidth: routeText.implicitWidth + 16
                    radius: 5
                    color: Qt.rgba(0, 0, 0, 0.28)
                    Text {
                        id: routeText
                        anchors.centerIn: parent
                        text: "↳ " + (panel.bus ? panel.bus.routeLabel : "")
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 9
                        font.weight: Font.Medium
                        color: "#b2b2b8"
                    }
                }
                Text {
                    text: panel.expanded ? panel.members.length + (panel.members.length === 1 ? " track" : " tracks")
                                         : "+" + panel.members.length
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 9
                    font.features: { "tnum": 1 }
                    color: "#9a9aa0"
                }
            }
        }

        Row {
            Layout.fillHeight: true
            spacing: 6

            Repeater {
                model: panel.expanded ? panel.shownMembers.length : 0
                ChannelStrip {
                    required property int index
                    width: panel.mixer.stripWidth
                    height: parent.height
                    session: panel.mixer.session
                    channel: panel.shownMembers[index] || ({})
                    outName: panel.bus ? panel.bus.name : "Master"
                    outColor: panel.bus ? Shared.Theme.trackColor(panel.bus.color) : Shared.Theme.trackColor("master")
                    barWidth: panel.mixer.barWidth
                    showInserts: panel.mixer.showInserts
                    showSends: panel.mixer.showSends
                    compact: panel.mixer.compact
                    wobbleL: panel.mixer.wobble(panel.groupIndex * 8 + index * 2)
                    wobbleR: panel.mixer.wobble(panel.groupIndex * 8 + index * 2 + 1)
                    onOpenDevices: id => panel.mixer.openDevices(id)
                }
            }

            ChannelStrip {
                visible: panel.busShown
                width: visible ? panel.mixer.stripWidth : 0
                height: parent.height
                isBus: true
                session: panel.mixer.session
                channel: panel.bus || ({})
                memberCount: panel.bus ? panel.bus.memberCount : 0
                outName: panel.bus ? panel.bus.output : "Master"
                outColor: Shared.Theme.trackColor("master")
                barWidth: panel.mixer.barWidth
                showInserts: panel.mixer.showInserts
                showSends: panel.mixer.showSends
                compact: panel.mixer.compact
                wobbleL: panel.mixer.wobble(panel.groupIndex * 8 + 6)
                wobbleR: panel.mixer.wobble(panel.groupIndex * 8 + 7)
                onOpenDevices: id => panel.mixer.openDevices(id)
            }
        }
    }
}
