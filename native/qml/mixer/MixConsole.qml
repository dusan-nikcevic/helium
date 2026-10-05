pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root

    property var session: Session
    property bool compact: false
    signal openDevices(string trackId)
    signal reviewRequested()

    property bool showInserts: !compact
    property bool showSends: !compact
    property bool showHistory: false
    property string sizeKey: "M"
    property var collapsed: ({})
    property string query: ""
    property alias scrollPosition: scroller.contentX

    readonly property var project: session ? session.project : null
    readonly property var groups: project && project.groups ? project.groups : []
    readonly property int stripWidth: ({ S: 100, M: 116, L: 150 })[sizeKey]
    readonly property int barWidth: ({ S: 5, M: 7, L: 11 })[sizeKey]
    readonly property bool allCollapsed: groups.length > 0 && groups.every(g => !!collapsed[g.id])
    readonly property int changeCount: root.session ? root.session.stagedAiPlan.length : 0

    function trackById(id) {
        const list = project ? project.tracks.concat(project.returns || []) : []
        for (let i = 0; i < list.length; i++)
            if (list[i].id === id) return list[i]
        return null
    }
    function busById(id) {
        const list = project ? project.buses : []
        for (let i = 0; i < list.length; i++)
            if (list[i].id === id) return list[i]
        return null
    }
    function matches(name, group) {
        const q = query.trim().toLowerCase()
        return q.length === 0 || name.toLowerCase().indexOf(q) >= 0 || group.name.toLowerCase().indexOf(q) >= 0
    }
    function toggleGroup(id) {
        const c = Object.assign({}, collapsed)
        c[id] = !c[id]
        collapsed = c
    }
    function setAllCollapsed(on) {
        const c = {}
        if (on) groups.forEach(g => c[g.id] = true)
        collapsed = c
    }
    function wobble(i) {
        return 0
    }

    color: Shared.Theme.panel
    clip: true


    component Divider: Rectangle {
        Layout.preferredWidth: 1
        Layout.preferredHeight: 18
        color: Qt.rgba(1, 1, 1, 0.09)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Shared.Theme.rail
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Qt.rgba(1, 1, 1, 0.07)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                spacing: root.compact ? 8 : 12

                Text {
                    text: "Console"
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.2
                    color: Shared.Theme.ink
                }

                RowLayout {
                    spacing: 3
                    Shared.DawButton {
                        text: "Inserts"
                        compact: true
                        checked: root.showInserts
                        tooltip: "Show insert racks"
                        onClicked: root.showInserts = !root.showInserts
                    }
                    Shared.DawButton {
                        text: "Sends"
                        compact: true
                        checked: root.showSends
                        tooltip: "Show send racks"
                        onClicked: root.showSends = !root.showSends
                    }
                }

                Divider {}

                Shared.DawButton {
                    text: root.allCollapsed ? "Expand All" : "Collapse All"
                    compact: true
                    onClicked: root.setAllCollapsed(!root.allCollapsed)
                }

                Divider {}

                RowLayout {
                    spacing: 6
                    Text {
                        text: "Width"
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 10
                        font.letterSpacing: 0.2
                        color: Shared.Theme.dim
                    }
                    Rectangle {
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: sizeRow.implicitWidth + 6
                        radius: 7
                        color: Shared.Theme.well
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.07)
                        Row {
                            id: sizeRow
                            anchors.centerIn: parent
                            spacing: 2
                            Repeater {
                                model: ["S", "M", "L"]
                                Rectangle {
                                    id: sizeOpt
                                    required property string modelData
                                    readonly property bool active: root.sizeKey === modelData
                                    width: 23; height: 16; radius: 5
                                    color: active ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                                    Text {
                                        anchors.centerIn: parent
                                        text: sizeOpt.modelData
                                        font.family: Shared.Theme.fontFamily
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        color: sizeOpt.active ? Shared.Theme.ink : Shared.Theme.muted
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.sizeKey = sizeOpt.modelData
                                    }
                                }
                            }
                        }
                    }
                }

                TextField {
                    id: search
                    Layout.preferredWidth: root.compact ? 120 : 150
                    Layout.preferredHeight: 22
                    leftPadding: 22
                    rightPadding: 8
                    topPadding: 0
                    bottomPadding: 0
                    placeholderText: "Search channels"
                    placeholderTextColor: Shared.Theme.dim
                    color: Shared.Theme.ink
                    selectionColor: Shared.Theme.alpha(Shared.Theme.accent, 0.4)
                    selectedTextColor: Shared.Theme.ink
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: 11
                    verticalAlignment: TextInput.AlignVCenter
                    onTextChanged: root.query = text
                    Keys.onEscapePressed: { text = ""; focus = false }
                    background: Rectangle {
                        radius: 7
                        color: Shared.Theme.well
                        border.width: 1
                        border.color: search.activeFocus ? Shared.Theme.alpha(Shared.Theme.accent, 0.5) : Qt.rgba(1, 1, 1, 0.07)
                        Text {
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: "⌕"
                            font.pixelSize: 12
                            color: Shared.Theme.dim
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Shared.DawButton {
                    objectName: "mixHistoryButton"
                    text: "History"
                    checked: root.showHistory
                    tooltip: "Inspect mixer edits and save mix snapshots"
                    onClicked: root.showHistory = !root.showHistory
                }

                Rectangle {
                    visible: root.changeCount > 0 && root.width > 1060
                    Layout.preferredHeight: 26
                    Layout.preferredWidth: aiRow.implicitWidth + 19
                    radius: 13
                    color: Shared.Theme.alpha(Shared.Theme.accent, 0.1)
                    border.width: 1
                    border.color: Shared.Theme.alpha(Shared.Theme.accent, 0.32)
                    RowLayout {
                        id: aiRow
                        anchors.verticalCenter: parent.verticalCenter
                        x: 13
                        spacing: 9
                        Shared.Sparkle {}
                        Text {
                            textFormat: Text.StyledText
                            text: (root.session.aiState === "applied" ? "Assistant changed " : "Assistant proposes ") + "<b><font color=\"#ffffff\">" + root.changeCount + " settings</font></b> in the "
                                  + (root.project ? root.project.section : "")
                            font.family: Shared.Theme.fontFamily
                            font.pixelSize: 12
                            color: "#9cc7ff"
                        }
                        Rectangle {
                            visible: root.session.aiState !== "applied"
                            Layout.preferredHeight: 18
                            Layout.preferredWidth: reviewText.implicitWidth + 20
                            radius: 9
                            color: reviewMouse.containsMouse ? Qt.rgba(0.486, 0.753, 1, 0.1) : "transparent"
                            border.width: 1
                            border.color: Qt.rgba(0.486, 0.753, 1, 0.3)
                            Text {
                                id: reviewText
                                anchors.centerIn: parent
                                text: "Review"
                                font.family: Shared.Theme.fontFamily
                                font.pixelSize: 11
                                color: "#7cc0ff"
                            }
                            MouseArea {
                                id: reviewMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.reviewRequested()
                            }
                        }
                        Text {
                            Layout.rightMargin: 8
                            text: "Undo"
                            font.family: Shared.Theme.fontFamily
                            font.pixelSize: 11
                            color: undoMouse.containsMouse && root.session.canUndo ? Shared.Theme.secondary : Shared.Theme.muted
                            opacity: root.session.canUndo ? 1 : 0.5
                            MouseArea {
                                id: undoMouse
                                anchors.fill: parent
                                anchors.margins: -4
                                hoverEnabled: true
                                enabled: root.session.canUndo
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.session.undo()
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                gradient: Gradient {
                    GradientStop { position: 0; color: "#19191b" }
                    GradientStop { position: 1; color: "#131315" }
                }

                Flickable {
                    id: scroller
                    anchors.fill: parent
                    contentWidth: groupRow.width + 2 * groupRow.x
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
                        id: groupRow
                        x: root.compact ? 10 : 16
                        y: root.compact ? 8 : 14
                        height: scroller.height - 2 * y
                        spacing: 14

                        Repeater {
                            model: root.groups.length
                            GroupPanel {
                                required property int index
                                height: groupRow.height
                                mixer: root
                                group: root.groups[index] || ({})
                                groupIndex: index
                            }
                        }
                    }

                    Text {
                        visible: groupRow.width === 0 && root.query.length > 0
                        anchors.centerIn: parent
                        text: "No channels match “" + root.query + "”"
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 12
                        color: Shared.Theme.muted
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 136 + 32
                z: 2
                gradient: Gradient {
                    GradientStop { position: 0; color: "#1a1a1c" }
                    GradientStop { position: 1; color: "#141416" }
                }

                Rectangle {
                    anchors.right: parent.left
                    width: 14
                    height: parent.height
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "transparent" }
                        GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.45) }
                    }
                }
                Rectangle {
                    width: 1
                    height: parent.height
                    color: Qt.rgba(1, 1, 1, 0.09)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: root.compact ? 8 : 14
                    anchors.bottomMargin: root.compact ? 16 : 22
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        radius: 8
                        color: Qt.rgba(1, 1, 1, 0.03)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.07)
                        Text {
                            anchors.centerIn: parent
                            text: "OUTPUT"
                            font.family: Shared.Theme.fontFamily
                            font.pixelSize: 8
                            font.weight: Font.Bold
                            font.letterSpacing: 1
                            color: Shared.Theme.dim
                        }
                    }

                    MasterStrip {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        session: root.session
                        channel: root.project ? root.project.master : ({})
                        barWidth: root.barWidth
                        showInserts: root.showInserts
                        compact: root.compact
                        wobbleL: root.wobble(40)
                        wobbleR: root.wobble(41)
                        onOpenDevices: id => root.openDevices(id)
                    }
                }
            }
        }
        MixHistoryPanel {
            objectName: "mixHistoryPanel"
            visible: root.showHistory
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(260, root.height * 0.45)
            session: root.session
        }
    }
}
