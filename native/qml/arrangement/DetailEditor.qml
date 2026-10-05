pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: root
    property var session: Session
    property string mode: "clip"
    signal modeChangedByUser(string mode)
    signal closeRequested()

    implicitHeight: 196
    color: "#141416"

    readonly property var hit: {
        var id = session ? session.selectedClipId : ""
        var tracks = session && session.project ? session.project.tracks || [] : []
        if (id) {
            for (var t = 0; t < tracks.length; t++) {
                var clips = tracks[t].clips || []
                for (var c = 0; c < clips.length; c++)
                    if (clips[c].id === id)
                        return { track: tracks[t], clip: clips[c] }
            }
        }
        if (session && session.selectedClip && session.selectedClip.id)
            return { track: session.selectedTrack || ({}), clip: session.selectedClip }
        return null
    }
    readonly property var clipData: hit ? hit.clip : ({})
    readonly property real clipStart: clipData.startBar !== undefined ? clipData.startBar : 1
    readonly property var trackData: hit ? hit.track : (session && session.selectedTrack ? session.selectedTrack : ({}))
    readonly property bool isMidi: clipData.type === "midi"
    readonly property color hue: Shared.Theme.trackColor(clipData.color || trackData.color || "")
    property int selectedNote: -1
    readonly property var note: isMidi && selectedNote >= 0 && clipData.notes ? clipData.notes[selectedNote] : null

    property string inspectedClipId: session.selectedClipId
    onInspectedClipIdChanged: selectedNote = -1

    function position(bar) {
        var whole = Math.floor(bar)
        var beats = (bar - whole) * 4
        var beat = Math.floor(beats + 1e-6)
        var tick = Math.floor((beats - beat) * 4 + 1e-6)
        return whole + "." + (beat + 1) + "." + (tick + 1)
    }
    function noteName(p) {
        return ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][p % 12] + (Math.floor(p / 12) - 1)
    }
    function bars(n) { return n + (n === 1 ? " bar" : " bars") }

    Accessible.role: Accessible.Pane
    Accessible.name: "Clip detail editor"

    Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

    Rectangle {
        id: header
        y: 1
        width: parent.width
        height: 34
        color: "#101012"
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.06) }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 9

            Rectangle {
                implicitWidth: 10
                implicitHeight: 10
                radius: 3
                color: root.hit ? root.hue : Shared.Theme.faint
            }
            Text {
                text: root.hit ? root.clipData.name : "No clip"
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.letterSpacing: -0.2
                color: Shared.Theme.ink
            }
            Text {
                text: "CLIP"
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.letterSpacing: 0.4
                color: Shared.Theme.dim
            }
            Text {
                text: !root.hit ? "Nothing selected"
                      : (root.trackData.name || "") + " · " + (root.isMidi ? "MIDI" : "Audio")
                        + " · " + root.bars(root.clipData.lengthBars)
                font.family: Shared.Theme.fontFamily
                font.pointSize: 10.5 * 0.75 // 10.5px
                color: Shared.Theme.muted
            }
            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                visible: root.isMidi
                text: root.note
                      ? root.noteName(root.note.pitch) + " · bar " + (root.note.bar + 1) + " beat "
                        + (root.note.beat + 1) + " · " + root.note.length + " beats"
                      : "Double-click the grid to add a note"
                font.family: Shared.Theme.fontFamily
                font.pointSize: 10.5 * 0.75 // 10.5px
                font.weight: root.note ? Font.DemiBold : Font.Normal
                color: root.note ? Shared.Theme.accent : Shared.Theme.dim
            }
            Item { Layout.fillWidth: true; visible: !root.isMidi }

            Shared.DawButton {
                objectName: "add-midi-note"
                visible: root.isMidi && !root.session.engineConnected && root.mode === "clip"
                text: "Add note"
                height: 24
                onClicked: root.session.addMidiNote({bar: 0, beat: 0, pitch: 60, length: 0.5, velocity: 100})
            }
            Shared.DawButton {
                objectName: "place-launcher-clip"
                visible: root.session.selectionOrigin === "launcher" && !!root.hit && !root.session.engineConnected
                text: "Place in arrangement"
                height: 24
                onClicked: {
                    var scenes = root.session.project.launcherScenes || []
                    for (var i = 0; i < scenes.length; ++i) {
                        var slots = scenes[i].slots || []
                        for (var j = 0; j < slots.length; ++j) {
                            if (slots[j].trackId === root.session.selectedTrackId && slots[j].clipId === root.session.selectedClipId) {
                                if (root.session.placeLauncherClip(scenes[i].id, root.session.selectedTrackId,
                                                                 Math.max(1, Math.floor(root.session.playheadBar))))
                                    root.session.setView("arrange")
                                return
                            }
                        }
                    }
                }
            }

            Rectangle {
                implicitWidth: tabRow.implicitWidth + 4
                implicitHeight: 24
                radius: 7
                color: Shared.Theme.well
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.07)
                Row {
                    id: tabRow
                    anchors.centerIn: parent
                    spacing: 2
                    Repeater {
                        model: [{ id: "clip", label: "Clip" }, { id: "devices", label: "Devices" }]
                        delegate: Button {
                            id: tab
                            required property var modelData
                            readonly property bool current: root.mode === modelData.id
                            text: modelData.label
                            implicitHeight: 20
                            implicitWidth: tabText.implicitWidth + 20
                            padding: 0
                            hoverEnabled: true
                            Accessible.name: modelData.label + " tab"
                            Accessible.checkable: true
                            Accessible.checked: current
                            onClicked: if (!current) root.modeChangedByUser(modelData.id)
                            background: Rectangle {
                                radius: 5
                                color: tab.current ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                                border.width: tab.visualFocus ? 1 : 0
                                border.color: Shared.Theme.accent
                            }
                            contentItem: Text {
                                id: tabText
                                text: tab.text
                                font.family: Shared.Theme.fontFamily
                                font.pixelSize: 11
                                font.weight: tab.current ? Font.DemiBold : Font.Medium
                                color: tab.current || tab.hovered ? Shared.Theme.ink : Shared.Theme.muted
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }

            Button {
                id: closeButton
                implicitWidth: 24
                implicitHeight: 24
                padding: 0
                hoverEnabled: true
                Accessible.name: "Close detail editor"
                onClicked: root.closeRequested()
                background: Rectangle {
                    radius: 6
                    color: Qt.rgba(1, 1, 1, closeButton.hovered ? 0.1 : 0.06)
                    border.width: closeButton.visualFocus ? 1 : 0
                    border.color: Shared.Theme.accent
                }
                contentItem: Text {
                    text: "✕"
                    font.pixelSize: 14
                    color: "#b6b6bb"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    Rectangle {
        id: body
        y: header.y + header.height
        width: parent.width
        height: parent.height - y
        gradient: Gradient {
            GradientStop { position: 0; color: Shared.Theme.panel }
            GradientStop { position: 1; color: "#1b1b1e" }
        }

        Column {
            visible: !root.hit
            anchors.centerIn: parent
            spacing: 5
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.trackData.name ? "No clip selected on " + root.trackData.name : "No clip selected"
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: Shared.Theme.secondary
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Click a clip in the arrangement to see its waveform or notes here."
                font.family: Shared.Theme.fontFamily
                font.pixelSize: 11
                color: Shared.Theme.muted
            }
        }

        RowLayout {
            visible: !!root.hit
            anchors.fill: parent
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 160
                Layout.fillHeight: true
                radius: 10
                color: Shared.Theme.strip
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)
                clip: true

                Rectangle {
                    id: cardTitle
                    x: 1
                    y: 1
                    width: parent.width - 2
                    height: 24
                    topLeftRadius: 9
                    topRightRadius: 9
                    color: root.hue
                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 40
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: root.clipData.name || ""
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        font.letterSpacing: -0.1
                        color: "#16161a"
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.isMidi ? "MIDI" : "AUDIO"
                        font.family: Shared.Theme.fontFamily
                        font.pixelSize: 8
                        font.weight: Font.Bold
                        font.letterSpacing: 0.3
                        color: Qt.rgba(0, 0, 0, 0.55)
                    }
                }

                Column {
                    anchors.top: cardTitle.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 11
                    anchors.topMargin: 8
                    spacing: 6

                    Repeater {
                        model: [
                            ["Track", root.trackData.name || "—"],
                            [root.session.selectionOrigin === "launcher" ? "Source start" : "Start", root.hit ? root.position(root.clipStart) : ""],
                            ["Length", root.hit ? root.bars(root.clipData.lengthBars) : ""],
                            ["End", root.hit ? root.position(root.clipStart + root.clipData.lengthBars) : ""],
                            root.isMidi ? ["Notes", String((root.clipData.notes || []).length)]
                                        : ["Seed", String(root.clipData.seed !== undefined ? root.clipData.seed : "—")]
                        ]
                        delegate: Item {
                            id: propRow
                            required property var modelData
                            width: parent.width
                            height: 12
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: propRow.modelData[0].toUpperCase()
                                font.family: Shared.Theme.fontFamily
                                font.pixelSize: 8
                                font.weight: Font.DemiBold
                                font.letterSpacing: 0.3
                                color: "#7a7a80"
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 48
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                text: propRow.modelData[1]
                                font.family: propRow.modelData[0] === "Track" ? Shared.Theme.fontFamily : Shared.Theme.monoFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: Shared.Theme.secondary
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: well
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: Shared.Theme.well
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.06)
                clip: true

                readonly property real contentX: root.isMidi ? 1 + 30 : 1
                readonly property real contentW: width - 2 - (root.isMidi ? 30 : 0)
                readonly property real pxPerBar: contentW / Math.max(1, root.clipData.lengthBars || 1)

                Item {
                    id: wellRuler
                    x: 1
                    y: 1
                    width: parent.width - 2
                    height: 15
                    Repeater {
                        model: Math.ceil(root.clipData.lengthBars || 0)
                        delegate: Text {
                            required property int index
                            x: well.contentX - 1 + index * well.pxPerBar + 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: Math.floor(root.clipStart) + index
                            font.family: Shared.Theme.fontFamily
                            font.pixelSize: 9
                            font.weight: Font.Medium
                            font.features: { "tnum": 1 }
                            color: "#7c7c82"
                        }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.06) }
                }

                Item {
                    id: editorArea
                    x: 1
                    y: wellRuler.y + wellRuler.height
                    width: parent.width - 2
                    height: parent.height - y - 1

                    Item {
                        anchors.fill: parent
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        visible: !root.isMidi
                        Waveform {
                            anchors.fill: parent
                            seed: root.clipData.seed || 0
                            color: Qt.tint(root.hue, Qt.rgba(1, 1, 1, 0.3))
                        }
                    }
                    PianoRoll {
                        anchors.fill: parent
                        visible: root.isMidi
                        notes: root.clipData.notes || []
                        lengthBars: root.clipData.lengthBars || 1
                        noteColor: root.hue
                        selectedIndex: root.selectedNote
                        onNoteTapped: (index) => root.selectedNote = index
                    }

                    Rectangle {
                        readonly property real offset: (root.session ? root.session.playheadBar : 0) - root.clipStart
                        visible: root.hit && root.session.selectionOrigin !== "launcher" && offset >= 0 && offset <= root.clipData.lengthBars
                        x: well.contentX - 1 + offset * well.pxPerBar
                        width: 1.5
                        height: parent.height
                        color: "#f4f4f6"
                    }
                }
            }
        }
    }
}
