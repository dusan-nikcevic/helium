pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: notesView
    property var notes: []
    property real pxPerBar: 52
    property color noteColor: "#bd9b62"
    clip: true

    readonly property real rowH: height / 12
    readonly property var range: {
        var lo = 127, hi = 0
        for (var i = 0; i < notes.length; i++) {
            lo = Math.min(lo, notes[i].pitch)
            hi = Math.max(hi, notes[i].pitch)
        }
        return notes.length ? { lo: lo, hi: hi } : { lo: 60, hi: 60 }
    }

    function rowOf(pitch) {
        var span = range.hi - range.lo
        return 1 + (span > 9 ? (range.hi - pitch) / span * 9 : (range.hi - pitch) + (9 - span) / 2)
    }

    Repeater {
        model: notesView.notes
        delegate: Rectangle {
            required property var modelData
            x: 2 + (modelData.bar + modelData.beat / 4) * notesView.pxPerBar
            y: (modelData.previewRow !== undefined ? modelData.previewRow : notesView.rowOf(modelData.pitch)) * notesView.rowH
            width: Math.max(3, modelData.length / 4 * notesView.pxPerBar - 3)
            height: notesView.rowH * (modelData.length >= 2 ? 1.4 : 0.92)
            radius: 1.5
            color: Qt.tint(notesView.noteColor, Qt.rgba(1, 1, 1, 0.1))
            border.width: 0.5
            border.color: Qt.rgba(notesView.noteColor.r, notesView.noteColor.g, notesView.noteColor.b, 0.65)
        }
    }
}
