pragma ComponentBehavior: Bound
import QtQuick
import "../shared" as Shared

Item {
    id: roll
    property var notes: []
    property real lengthBars: 1
    property color noteColor: "#bd9b62"
    property int selectedIndex: -1
    signal noteTapped(int index)

    readonly property int keyWidth: 30
    readonly property var names: ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    readonly property var range: {
        var lo = 127, hi = 0
        for (var i = 0; i < notes.length; i++) {
            lo = Math.min(lo, notes[i].pitch)
            hi = Math.max(hi, notes[i].pitch)
        }
        if (!notes.length) { lo = 60; hi = 60 }
        lo -= 2; hi += 2
        var missing = 13 - (hi - lo + 1)
        if (missing > 0) { lo -= Math.floor(missing / 2); hi += Math.ceil(missing / 2) }
        return { lo: lo, hi: hi }
    }
    readonly property int rowCount: range.hi - range.lo + 1
    readonly property real rowH: height / rowCount
    readonly property real gridWidth: width - keyWidth
    readonly property real pxPerBar: gridWidth / Math.max(1, lengthBars)

    function isBlack(p) { return [1, 3, 6, 8, 10].indexOf(p % 12) >= 0 }
    function noteName(p) { return names[p % 12] + (Math.floor(p / 12) - 1) }

    Repeater {
        model: roll.rowCount
        delegate: Item {
            id: keyRow
            required property int index
            readonly property int pitch: roll.range.hi - index
            y: index * roll.rowH
            width: roll.width
            height: roll.rowH
            Rectangle {
                width: roll.keyWidth
                height: parent.height
                color: roll.isBlack(keyRow.pitch) ? "#1c1c1f" : "#c9c9ce"
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Qt.rgba(0, 0, 0, 0.35)
                }
                Text {
                    visible: keyRow.pitch % 12 === 0 && roll.rowH >= 6
                    anchors.right: parent.right
                    anchors.rightMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    text: roll.noteName(keyRow.pitch)
                    font.family: Shared.Theme.fontFamily
                    font.pixelSize: Math.min(8, roll.rowH)
                    font.weight: Font.DemiBold
                    color: "#3a3a40"
                }
            }
            Rectangle {
                x: roll.keyWidth
                width: roll.gridWidth
                height: parent.height
                color: roll.isBlack(keyRow.pitch) ? "transparent" : Qt.rgba(1, 1, 1, 0.025)
                Rectangle {
                    visible: keyRow.pitch % 12 === 0
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.07)
                }
            }
        }
    }

    Item {
        id: grid
        x: roll.keyWidth
        width: roll.gridWidth
        height: roll.height
        clip: true

        TapHandler { onTapped: roll.noteTapped(-1) }

        Repeater {
            model: Math.ceil(roll.lengthBars) * 4
            delegate: Rectangle {
                required property int index
                x: index * roll.pxPerBar / 4
                width: 1
                height: grid.height
                color: index % 4 === 0 ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(1, 1, 1, 0.025)
            }
        }

        Repeater {
            model: roll.notes
            delegate: Rectangle {
                id: noteRect
                required property var modelData
                required property int index
                readonly property bool picked: index === roll.selectedIndex
                x: (modelData.bar + modelData.beat / 4) * roll.pxPerBar
                y: (roll.range.hi - modelData.pitch) * roll.rowH + 0.5
                width: Math.max(3, modelData.length / 4 * roll.pxPerBar - 1)
                height: Math.max(2, roll.rowH - 1)
                radius: 1.5
                color: picked ? Shared.Theme.accent : Qt.tint(roll.noteColor, Qt.rgba(1, 1, 1, 0.1))
                border.width: picked ? 1.5 : 0.5
                border.color: picked ? "#bfe0ff" : Shared.Theme.alpha(roll.noteColor, 0.65)
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: roll.noteName(modelData.pitch) + " at bar " + (modelData.bar + 1)
                                 + " beat " + (modelData.beat + 1) + ", " + modelData.length + " beats"
                Keys.onSpacePressed: roll.noteTapped(index)
                Keys.onReturnPressed: roll.noteTapped(index)
                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: roll.noteTapped(noteRect.index)
                }
            }
        }
    }
}
