pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Zephyr 1.0
import "../shared" as Shared

FocusScope {
    id: root
    property var session: Session
    signal openDevices(string trackId)

    readonly property var project: session && session.project ? session.project : ({})
    readonly property var tracks: project.tracks || []
    readonly property var sections: project.sections || []
    readonly property real originBar: project.startBar !== undefined ? project.startBar : 9
    readonly property int totalBars: project.totalBars || 14

    readonly property int headerWidth: 150
    readonly property int laneHeight: 58
    property real zoom: 1
    readonly property real pxPerBar: 52 * zoom
    readonly property real timelineWidth: totalBars * pxPerBar
    readonly property real tracksHeight: tracks.length * laneHeight
    readonly property real columnHeight: tracksHeight + 40
    readonly property real snapStep: session && session.snapEnabled ? 0.25 : 1 / 16
    readonly property var highlightSection: {
        for (var i = 0; i < sections.length; i++)
            if (sections[i].highlight)
                return sections[i]
        return null
    }

    function xForBar(bar) { return (bar - originBar) * pxPerBar }
    function snapBar(bar) { return Math.max(1, Math.round(bar / snapStep) * snapStep) }

    function setZoom(z, anchorX) {
        z = Math.max(0.5, Math.min(3, z))
        if (z === zoom)
            return
        var ax = anchorX === undefined ? lanes.width / 2 : anchorX
        var bar = (lanes.contentX + ax) / pxPerBar
        zoom = z
        lanes.contentX = Math.max(0, Math.min(bar * pxPerBar - ax, lanes.contentWidth - lanes.width))
    }

    function findSelected() {
        for (var t = 0; t < tracks.length; t++) {
            var clips = tracks[t].clips || []
            for (var c = 0; c < clips.length; c++)
                if (clips[c].id === session.selectedClipId)
                    return { track: tracks[t], clip: clips[c], trackIndex: t }
        }
        return null
    }

    function nudgeSelected(deltaBars) {
        var hit = findSelected()
        if (!hit)
            return
        session.moveClip(hit.track.id, hit.clip.id, snapBar(hit.clip.startBar + deltaBars))
        root.forceActiveFocus()
    }

    function selectAdjacentTrack(step) {
        if (!tracks.length)
            return
        var idx = -1
        for (var i = 0; i < tracks.length; i++)
            if (tracks[i].id === session.selectedTrackId)
                idx = i
        idx = Math.max(0, Math.min(tracks.length - 1, idx + step))
        session.selectTrack(tracks[idx].id)
        lanes.ensureRowVisible(idx)
    }

    Accessible.role: Accessible.Pane
    Accessible.name: "Arrangement"

    Keys.onPressed: (event) => {
        var ctrl = event.modifiers & Qt.ControlModifier
        if (ctrl && (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal)) {
            setZoom(zoom * 1.25); event.accepted = true
        } else if (ctrl && event.key === Qt.Key_Minus) {
            setZoom(zoom / 1.25); event.accepted = true
        } else if (ctrl && event.key === Qt.Key_0) {
            setZoom(1); event.accepted = true
        } else if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            var step = (event.modifiers & Qt.ShiftModifier) ? 1 : snapStep
            nudgeSelected(event.key === Qt.Key_Left ? -step : step)
            event.accepted = true
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            selectAdjacentTrack(event.key === Qt.Key_Up ? -1 : 1)
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Shared.Theme.panel
    }

    Item {
        id: sectionRow
        width: parent.width
        height: 24
        Rectangle {
            x: root.headerWidth - 1
            width: 1
            height: parent.height
            color: Qt.rgba(1, 1, 1, 0.07)
        }
        Item {
            x: root.headerWidth
            width: parent.width - x
            height: parent.height
            clip: true
            Item {
                x: -lanes.contentX
                width: root.timelineWidth
                height: parent.height
                Repeater {
                    model: root.sections
                    delegate: Rectangle {
                        id: pill
                        required property var modelData
                        required property int index
                        readonly property bool active: !!modelData.highlight
                        x: root.xForBar(modelData.startBar) + 4
                        y: 4
                        width: modelData.lengthBars * root.pxPerBar - (index === root.sections.length - 1 ? 12 : 10)
                        height: 16
                        radius: 5
                        color: active ? Shared.Theme.alpha(Shared.Theme.accent, 0.12) : Qt.rgba(1, 1, 1, 0.05)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: modelData.name + " section, bars " + modelData.startBar + " to " + (modelData.startBar + modelData.lengthBars - 1)
                        Text {
                            anchors.fill: parent
                            text: pill.modelData.name.toUpperCase()
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            font.family: Shared.Theme.fontFamily
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.3
                            color: pill.active ? Shared.Theme.accent : Shared.Theme.muted
                        }
                    }
                }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.05)
        }
    }

    Rectangle {
        id: ruler
        y: sectionRow.height
        width: parent.width
        height: 24
        color: Shared.Theme.rail
        Text {
            x: 10
            width: root.headerWidth - 20
            height: parent.height - 1
            verticalAlignment: Text.AlignVCenter
            text: "BARS"
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 10
            font.letterSpacing: 0.3
            color: Shared.Theme.dim
        }
        Rectangle {
            x: root.headerWidth - 1
            width: 1
            height: parent.height
            color: Qt.rgba(1, 1, 1, 0.07)
        }
        Item {
            x: root.headerWidth
            width: parent.width - x
            height: parent.height
            clip: true
            Item {
                id: rulerContent
                x: -lanes.contentX
                width: root.timelineWidth
                height: parent.height
                Repeater {
                    model: root.totalBars
                    delegate: Text {
                        required property int index
                        x: index * root.pxPerBar + 6
                        y: 6
                        text: root.originBar + index
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 10
                        font.weight: Font.Medium
                        font.features: { "tnum": 1 }
                        color: "#7c7c82"
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    Accessible.role: Accessible.Slider
                    Accessible.name: "Bar ruler, click to move the playhead"
                    onClicked: (mouse) => root.session.seek(root.snapBar(root.originBar + mouse.x / root.pxPerBar))
                }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.08)
        }
    }

    Item {
        id: body
        y: ruler.y + ruler.height
        width: parent.width
        height: parent.height - y

        Flickable {
            id: headerColumn
            objectName: "headerColumn"
            width: root.headerWidth - 1
            height: parent.height
            contentHeight: root.columnHeight
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            clip: true
            onContentYChanged: lanes.contentY = contentY

            Column {
                width: parent.width
                Repeater {
                    model: root.tracks.length
                    delegate: TrackHeader {
                        required property int index
                        readonly property var modelData: root.tracks[index] || ({})
                        width: headerColumn.width
                        session: root.session
                        track: modelData
                        onOpenDevices: (trackId) => root.openDevices(trackId)
                    }
                }
                Item {
                    width: headerColumn.width
                    height: 40
                    Accessible.role: Accessible.StaticText
                    Accessible.name: "Add Track (not available yet)"
                    Row {
                        x: 13
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 7
                        Text {
                            text: "+"
                            anchors.verticalCenter: parent.verticalCenter
                            font.family: Shared.Theme.fontFamily
                            font.pixelSize: 14
                            color: "#5a5a5f"
                        }
                        Text {
                            text: "Add Track"
                            anchors.verticalCenter: parent.verticalCenter
                            font.family: Shared.Theme.fontFamily
                            font.pointSize: 11.5 * 0.75 // 11.5px
                            color: "#5a5a5f"
                        }
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Qt.rgba(1, 1, 1, 0.04)
                    }
                }
            }
        }
        Rectangle {
            x: root.headerWidth - 1
            width: 1
            height: parent.height
            color: Qt.rgba(1, 1, 1, 0.07)
        }

        Flickable {
            id: lanes
            objectName: "lanes"
            x: root.headerWidth
            width: parent.width - x
            height: parent.height
            contentWidth: root.timelineWidth
            contentHeight: root.columnHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            onContentYChanged: headerColumn.contentY = contentY
            ScrollBar.horizontal: ThinScrollBar {}
            ScrollBar.vertical: ThinScrollBar {}

            function ensureRowVisible(i) {
                var top = i * root.laneHeight
                if (top < contentY)
                    contentY = top
                else if (top + root.laneHeight > contentY + height)
                    contentY = Math.min(top + root.laneHeight - height, contentHeight - height)
            }

            WheelHandler {
                acceptedModifiers: Qt.ControlModifier
                onWheel: (event) => root.setZoom(root.zoom * Math.pow(1.0015, event.angleDelta.y), point.position.x)
            }

            Item {
                width: root.timelineWidth
                height: root.tracksHeight

                Rectangle {
                    visible: root.highlightSection !== null
                    x: root.highlightSection ? root.xForBar(root.highlightSection.startBar) : 0
                    width: root.highlightSection ? root.highlightSection.lengthBars * root.pxPerBar : 0
                    height: parent.height
                    color: Shared.Theme.alpha(Shared.Theme.accent, 0.045)
                    Rectangle {
                        width: 1
                        height: parent.height
                        color: Shared.Theme.alpha(Shared.Theme.accent, 0.28)
                    }
                }

                Repeater {
                    model: root.totalBars + 1
                    delegate: Item {
                        id: barLine
                        required property int index
                        x: index * root.pxPerBar
                        height: parent.height
                        Rectangle {
                            width: 1
                            height: parent.height
                            color: Qt.rgba(1, 1, 1, 0.07)
                        }
                        Repeater {
                            model: barLine.index < root.totalBars ? 3 : 0
                            delegate: Rectangle {
                                required property int index
                                x: (index + 1) * root.pxPerBar / 4
                                width: 1
                                height: parent.height
                                color: Qt.rgba(1, 1, 1, 0.025)
                            }
                        }
                    }
                }

                Repeater {
                    model: root.tracks.length
                    delegate: Item {
                        id: lane
                        required property int index
                        readonly property var modelData: root.tracks[index] || ({clips: []})
                        y: index * root.laneHeight
                        width: parent.width
                        height: root.laneHeight

                        TapHandler { onTapped: root.session.selectTrack(lane.modelData.id) }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.05)
                        }

                        Repeater {
                            model: (lane.modelData.clips || []).length
                            delegate: ClipBlock {
                                id: clipItem
                                objectName: "clip-" + modelData.id
                                required property int index
                                readonly property var modelData: (lane.modelData.clips || [])[index] || ({})
                                property real dragBars: 0
                                readonly property real startBar: modelData.startBar + dragBars

                                clipData: modelData
                                selected: root.session.selectedClipId === modelData.id
                                pxPerBar: root.pxPerBar
                                x: root.xForBar(startBar)
                                y: 3
                                z: selected || drag.active ? 4 : 1
                                width: modelData.lengthBars * root.pxPerBar - 4
                                height: root.laneHeight - 6
                                opacity: drag.active ? 0.92 : 1

                                activeFocusOnTab: true
                                Accessible.role: Accessible.Button
                                Accessible.name: modelData.name + " clip on " + lane.modelData.name
                                                 + ", bar " + modelData.startBar + ", " + modelData.lengthBars + " bars"
                                Accessible.selectable: true
                                Accessible.selected: selected
                                Keys.onSpacePressed: root.session.selectClip(lane.modelData.id, modelData.id)
                                Keys.onReturnPressed: root.session.selectClip(lane.modelData.id, modelData.id)

                                Rectangle {
                                    anchors.fill: parent
                                    visible: clipItem.activeFocus && !clipItem.selected
                                    radius: parent.radius
                                    color: "transparent"
                                    border.width: 1
                                    border.color: Qt.rgba(1, 1, 1, 0.6)
                                }

                                HoverHandler { cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor }
                                // why: Exclusive grabs keep clip taps from selecting the background lane.
                                TapHandler {
                                    gesturePolicy: TapHandler.ReleaseWithinBounds
                                    onTapped: root.session.selectClip(lane.modelData.id, clipItem.modelData.id)
                                }
                                DragHandler {
                                    id: drag
                                    target: null
                                    yAxis.enabled: false
                                    onActiveChanged: {
                                        if (active) {
                                            root.session.selectClip(lane.modelData.id, clipItem.modelData.id)
                                            return
                                        }
                                        var moved = clipItem.dragBars
                                        if (moved !== 0)
                                            root.session.moveClip(lane.modelData.id, clipItem.modelData.id,
                                                                  clipItem.modelData.startBar + moved)
                                        clipItem.dragBars = 0
                                    }
                                    onTranslationChanged: {
                                        var start = clipItem.modelData.startBar
                                        clipItem.dragBars = root.snapBar(start + translation.x / root.pxPerBar) - start
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    x: root.xForBar(root.session ? root.session.playheadBar : root.originBar)
                    z: 6
                    width: 1.5
                    height: parent.height
                    color: "#f4f4f6"
                    Rectangle {
                        x: -4
                        width: 9
                        height: 6
                        radius: 1
                        color: "#f4f4f6"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 16
                        height: parent.height
                        z: -1
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: "transparent" }
                            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.22) }
                            GradientStop { position: 1; color: "transparent" }
                        }
                    }
                }
            }
        }

        Text {
            visible: root.tracks.length === 0
            anchors.centerIn: lanes
            text: "No tracks in this project"
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 12
            color: Shared.Theme.muted
        }
    }
}
