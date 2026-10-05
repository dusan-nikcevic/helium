pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root
    property var session: Session

    readonly property var project: session && session.project ? session.project : ({})
    readonly property var tracks: project.tracks || []
    readonly property var scenes: project.launcherScenes || []
    readonly property int gutterWidth: 150
    readonly property int columnWidth: 120
    readonly property int headerHeight: 40
    readonly property int rowHeight: 58
    readonly property var clipIndex: {
        var map = {}
        if (project.clipSources) {
            for (const source of project.clipSources) map[source.trackId + "/" + source.id] = source
            return map
        }
        for (var t = 0; t < tracks.length; t++) {
            var clips = tracks[t].clips || []
            for (var c = 0; c < clips.length; c++)
                map[tracks[t].id + "/" + clips[c].id] = clips[c]
        }
        return map
    }

    function slotClip(scene, trackId) {
        var slots = scene.slots || []
        for (var i = 0; i < slots.length; i++)
            if (slots[i].trackId === trackId && slots[i].clipId)
                return clipIndex[trackId + "/" + slots[i].clipId] || null
        return null
    }
    function filledCount(scene) {
        var n = 0
        for (var t = 0; t < tracks.length; t++)
            if (slotClip(scene, tracks[t].id))
                n++
        return n
    }
    function pick(sceneId, trackId, clip) {
        session.selectLauncherSlot(sceneId, trackId)
        if (clip && session.selectedClipId !== clip.id)
            session.selectClip(trackId, clip.id)
    }

    color: Shared.Theme.panel
    Accessible.role: Accessible.Table
    Accessible.name: "Clip launcher"

    Rectangle {
        id: header
        width: parent.width
        height: root.headerHeight
        color: Shared.Theme.rail

        Row {
            x: 10
            height: parent.height - 1
            spacing: 6
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "SCENES"
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 10
                font.letterSpacing: 0.3
                color: Shared.Theme.dim
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.scenes.length
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 10
                color: Shared.Theme.faint
            }
        }
        Rectangle { x: root.gutterWidth - 1; width: 1; height: parent.height; color: Qt.rgba(1, 1, 1, 0.07) }

        Item {
            x: root.gutterWidth
            width: parent.width - x
            height: parent.height
            clip: true
            Row {
                x: -grid.contentX
                Repeater {
                    model: root.tracks
                    delegate: Item {
                        id: column
                        required property var modelData
                        readonly property color hue: Shared.Theme.trackColor(modelData.color || "")
                        readonly property bool current: root.session.selectedTrackId === modelData.id
                        width: root.columnWidth
                        height: root.headerHeight
                        Accessible.role: Accessible.ColumnHeader
                        Accessible.name: modelData.name

                        TapHandler { onTapped: root.session.selectTrack(column.modelData.id) }
                        Rectangle { x: 3; y: 0; width: parent.width - 6; height: 2; radius: 1; color: column.hue }
                        Rectangle {
                            id: badge
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(18, badgeText.implicitWidth + 6)
                            height: 16
                            radius: 4
                            color: Shared.Theme.alpha(column.hue, 0.9)
                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: (column.modelData.short || "").toUpperCase()
                                font.family: Shared.Theme.fontFamily
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                color: "#17171b"
                            }
                        }
                        Text {
                            anchors.left: badge.right
                            anchors.leftMargin: 7
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: column.modelData.name
                            elide: Text.ElideRight
                            font.family: Shared.Theme.fontFamily
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            font.letterSpacing: -0.2
                            color: column.current ? Shared.Theme.ink : "#e8e8ea"
                        }
                        Rectangle {
                            visible: column.current
                            anchors.bottom: parent.bottom
                            x: 3
                            width: parent.width - 6
                            height: 2
                            color: Shared.Theme.accent
                        }
                        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Qt.rgba(1, 1, 1, 0.04) }
                    }
                }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }
    }

    Item {
        id: body
        y: header.height
        width: parent.width
        height: parent.height - y
        visible: root.scenes.length > 0 && root.tracks.length > 0

        Flickable {
            id: gutter
            width: root.gutterWidth - 1
            height: parent.height
            contentHeight: root.scenes.length * root.rowHeight
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            clip: true
            onContentYChanged: grid.contentY = contentY

            Column {
                Repeater {
                    model: root.scenes
                    delegate: Rectangle {
                        id: sceneRow
                        required property var modelData
                        required property int index
                        readonly property bool active: root.session.activeSceneId === modelData.id
                        width: gutter.width
                        height: root.rowHeight
                        color: active ? "#1b2230" : "#1a1a1c"

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.05) }

                        Button {
                            id: launch
                            x: 11
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 22
                            implicitHeight: 22
                            padding: 0
                            hoverEnabled: true
                            Accessible.name: "Set " + sceneRow.modelData.name + " as the active scene"
                            ToolTip.visible: hovered
                            ToolTip.delay: 500
                            ToolTip.text: "Mark as active scene"
                            onClicked: root.session.launchScene(sceneRow.modelData.id)
                            background: Rectangle {
                                radius: 11
                                color: sceneRow.active ? Shared.Theme.accent
                                                       : Qt.rgba(1, 1, 1, launch.hovered ? 0.08 : 0)
                                border.width: sceneRow.active ? 0 : 1
                                border.color: launch.visualFocus ? Shared.Theme.accent : Qt.rgba(1, 1, 1, 0.12)
                            }
                            contentItem: Item {
                                Shape {
                                    anchors.centerIn: parent
                                    width: 8
                                    height: 9
                                    preferredRendererType: Shape.CurveRenderer
                                    ShapePath {
                                        strokeWidth: 0
                                        strokeColor: "transparent"
                                        fillColor: sceneRow.active ? "#ffffff" : Shared.Theme.muted
                                        startX: 1; startY: 0
                                        PathLine { x: 8; y: 4.5 }
                                        PathLine { x: 1; y: 9 }
                                        PathLine { x: 1; y: 0 }
                                    }
                                }
                            }
                        }

                        Column {
                            anchors.left: launch.right
                            anchors.leftMargin: 9
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 3
                            Row {
                                spacing: 6
                                width: parent.width
                                Text {
                                    width: Math.min(implicitWidth, parent.width - (activeTag.visible ? activeTag.width + 6 : 0))
                                    text: sceneRow.modelData.name
                                    elide: Text.ElideRight
                                    font.family: Shared.Theme.fontFamily
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    font.letterSpacing: -0.2
                                    color: sceneRow.active ? Shared.Theme.ink : "#e8e8ea"
                                }
                                Rectangle {
                                    id: activeTag
                                    visible: sceneRow.active
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: activeText.implicitWidth + 10
                                    height: 14
                                    radius: 4
                                    color: Shared.Theme.alpha(Shared.Theme.accent, 0.14)
                                    Text {
                                        id: activeText
                                        anchors.centerIn: parent
                                        text: "ACTIVE"
                                        font.family: Shared.Theme.fontFamily
                                        font.pixelSize: 8
                                        font.weight: Font.Bold
                                        font.letterSpacing: 0.4
                                        color: Shared.Theme.accent
                                    }
                                }
                            }
                            Text {
                                property int filled: root.filledCount(sceneRow.modelData)
                                text: filled === 0 ? "Empty scene" : filled + (filled === 1 ? " clip" : " clips")
                                font.family: Shared.Theme.fontFamily
                                font.pixelSize: 10
                                color: Shared.Theme.dim
                            }
                        }
                    }
                }
            }
        }
        Rectangle { x: root.gutterWidth - 1; width: 1; height: parent.height; color: Qt.rgba(1, 1, 1, 0.07) }

        Flickable {
            id: grid
            x: root.gutterWidth
            width: parent.width - x
            height: parent.height
            contentWidth: root.tracks.length * root.columnWidth
            contentHeight: root.scenes.length * root.rowHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            onContentYChanged: gutter.contentY = contentY
            ScrollBar.horizontal: ThinScrollBar {}
            ScrollBar.vertical: ThinScrollBar {}

            Column {
                Repeater {
                    model: root.scenes
                    delegate: Item {
                        id: row
                        required property var modelData
                        readonly property bool active: root.session.activeSceneId === modelData.id
                        width: grid.contentWidth
                        height: root.rowHeight

                        Rectangle {
                            visible: row.active
                            anchors.fill: parent
                            color: Shared.Theme.alpha(Shared.Theme.accent, 0.045)
                            Rectangle { width: 1; height: parent.height; color: Shared.Theme.alpha(Shared.Theme.accent, 0.28) }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.05) }

                        Row {
                            Repeater {
                                model: root.tracks
                                delegate: Item {
                                    id: slot
                                    required property var modelData
                                    readonly property var slotClipData: root.slotClip(row.modelData, modelData.id)
                                    width: root.columnWidth
                                    height: root.rowHeight
                                    activeFocusOnTab: true
                                    Accessible.role: Accessible.Cell
                                    Accessible.name: row.modelData.name + ", " + modelData.name + ": "
                                                     + (slot.slotClipData ? slot.slotClipData.name + " clip" : "empty slot")
                                    Keys.onSpacePressed: root.pick(row.modelData.id, modelData.id, slot.slotClipData)
                                    Keys.onReturnPressed: root.pick(row.modelData.id, modelData.id, slot.slotClipData)

                                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Qt.rgba(1, 1, 1, 0.03) }

                                    HoverHandler { id: hover }
                                    TapHandler { onTapped: root.pick(row.modelData.id, slot.modelData.id, slot.slotClipData) }

                                    ClipBlock {
                                        visible: slot.slotClipData !== null
                                        x: 3
                                        y: 3
                                        width: parent.width - 6
                                        height: parent.height - 6
                                        clipData: slot.slotClipData || ({})
                                        selected: slot.slotClipData !== null && root.session.selectedClipId === slot.slotClipData.id
                                        pxPerBar: width / Math.max(1, slot.slotClipData ? slot.slotClipData.lengthBars : 1)
                                    }

                                    Shape {
                                        visible: slot.slotClipData === null
                                        x: 3
                                        y: 3
                                        width: parent.width - 6
                                        height: parent.height - 6
                                        ShapePath {
                                            strokeWidth: 1
                                            strokeColor: Qt.rgba(1, 1, 1, hover.hovered ? 0.16 : 0.08)
                                            strokeStyle: ShapePath.DashLine
                                            dashPattern: [3, 3]
                                            fillColor: "transparent"
                                            PathRectangle { x: 0.5; y: 0.5; width: slot.width - 7; height: slot.height - 7; radius: 5 }
                                        }
                                    }
                                    Rectangle {
                                        visible: slot.slotClipData === null
                                        anchors.centerIn: parent
                                        width: 4
                                        height: 4
                                        radius: 2
                                        color: Qt.rgba(1, 1, 1, 0.1)
                                    }

                                    Rectangle {
                                        visible: slot.activeFocus
                                        x: 2
                                        y: 2
                                        width: parent.width - 4
                                        height: parent.height - 4
                                        radius: 6
                                        color: "transparent"
                                        border.width: 1
                                        border.color: Qt.rgba(1, 1, 1, 0.6)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Column {
        visible: !body.visible
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.headerHeight / 2
        spacing: 5
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.tracks.length === 0 ? "No tracks yet" : "No scenes yet"
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: Shared.Theme.secondary
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.tracks.length === 0
                  ? "Tracks in the project show up here as launcher columns."
                  : "Scenes in the project's launcher show up here as rows of clip slots."
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 11
            color: Shared.Theme.muted
        }
    }
}
