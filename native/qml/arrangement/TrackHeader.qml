pragma ComponentBehavior: Bound
import QtQuick
import "../shared" as Shared

Rectangle {
    id: row
    property var session
    property var track: ({})
    signal openDevices(string trackId)

    readonly property color hue: Shared.Theme.trackColor(track.color || "")
    readonly property real level: Math.max(track.meterL || 0, track.meterR || 0)

    width: 150
    height: 58
    color: "#1a1a1c"
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: (track.name || "") + " track"
    Accessible.description: "Enter opens the device chain, Space selects the track"

    Keys.onReturnPressed: row.openDevices(track.id)
    Keys.onEnterPressed: row.openDevices(track.id)
    Keys.onSpacePressed: session.selectTrack(track.id)

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: row.session.selectTrack(row.track.id)
        onDoubleClicked: row.openDevices(row.track.id)
    }

    Rectangle { width: 3; height: parent.height; color: row.hue }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.05) }

    Rectangle {
        id: badge
        x: 13
        y: 7
        width: Math.max(18, badgeText.implicitWidth + 6)
        height: 16
        radius: 4
        color: Shared.Theme.alpha(row.hue, 0.9)
        Text {
            id: badgeText
            anchors.centerIn: parent
            text: (row.track.short || "").toUpperCase()
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
        anchors.rightMargin: 8
        anchors.verticalCenter: badge.verticalCenter
        text: row.track.name || ""
        elide: Text.ElideRight
        font.family: Shared.Theme.fontFamily
        font.pixelSize: 12
        font.weight: Font.Medium
        font.letterSpacing: -0.2
        color: "#e8e8ea"
    }

    SmallToggle {
        id: muteChip
        x: 13
        y: row.height - 7 - height
        text: "M"
        on: !!row.track.muted
        Accessible.name: "Mute " + (row.track.name || "")
        onClicked: row.session.setTrackValue(row.track.id, "muted", !row.track.muted)
    }
    SmallToggle {
        id: soloChip
        x: muteChip.x + muteChip.width + 5
        y: muteChip.y
        text: "S"
        on: !!row.track.soloed
        onColor: "#e8c466"
        onTextColor: "#1a1a1c"
        Accessible.name: "Solo " + (row.track.name || "")
        onClicked: row.session.setTrackValue(row.track.id, "soloed", !row.track.soloed)
    }
    Rectangle {
        x: soloChip.x + soloChip.width + 7
        width: row.width - 8 - x
        anchors.verticalCenter: muteChip.verticalCenter
        height: 4
        radius: 2
        color: Qt.rgba(1, 1, 1, 0.07)
        Accessible.role: Accessible.ProgressBar
        Accessible.name: "Level " + Math.round(row.level * 100) + "%"
        Rectangle {
            width: parent.width * row.level
            height: parent.height
            radius: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Shared.Theme.success }
                GradientStop { position: 1; color: row.level > 0.6 ? "#e8c466" : Shared.Theme.success }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: row.activeFocus
        color: "transparent"
        border.width: 1
        border.color: Shared.Theme.alpha(Shared.Theme.accent, 0.7)
    }
}
