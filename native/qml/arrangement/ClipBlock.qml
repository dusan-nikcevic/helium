pragma ComponentBehavior: Bound
import QtQuick
import Zephyr 1.0
import "../shared" as Shared

Rectangle {
    id: block
    property var clipData: ({})
    property bool selected: false
    property real pxPerBar: 52
    readonly property color hue: Shared.Theme.trackColor(clipData.color || "")
    readonly property real edge: selected ? 1.5 : 1

    radius: 5
    color: Shared.Theme.alpha(hue, 0.16)
    border.width: edge
    border.color: selected ? Shared.Theme.accent : Shared.Theme.alpha(hue, 0.5)

    Rectangle {
        visible: block.selected
        anchors.fill: parent
        anchors.margins: -3
        radius: block.radius + 3
        color: "transparent"
        border.width: 3
        border.color: Shared.Theme.alpha(Shared.Theme.accent, 0.18)
    }

    Rectangle {
        id: header
        x: block.edge
        y: block.edge
        width: parent.width - 2 * block.edge
        height: 13
        topLeftRadius: block.radius - block.edge
        topRightRadius: block.radius - block.edge
        color: block.selected ? Shared.Theme.accent : block.hue
        Text {
            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            text: block.clipData.name || ""
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            font.family: Shared.Theme.fontFamily
            font.pixelSize: 9
            font.weight: Font.DemiBold
            font.letterSpacing: 0.2
            color: block.selected ? "#ffffff" : "#17171b"
        }
    }

    Item {
        id: body
        x: block.edge
        y: header.y + header.height
        width: header.width
        height: parent.height - y - block.edge
        clip: true

        Waveform {
            anchors.fill: parent
            visible: block.clipData.type === "audio"
            seed: block.clipData.seed || 0
            color: Qt.tint(block.hue, Qt.rgba(1, 1, 1, 0.3))
        }
        MidiNotes {
            anchors.fill: parent
            visible: block.clipData.type === "midi"
            notes: block.clipData.notes || []
            pxPerBar: block.pxPerBar
            noteColor: block.hue
        }
    }
}
