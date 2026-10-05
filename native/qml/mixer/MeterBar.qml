pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: bar

    property real level: 0
    property real peak: 0
    readonly property int segments: 18
    readonly property int peakIndex: Math.min(segments - 1, Math.floor((Math.max(peak, level) + 0.05) * segments))

    implicitWidth: 7
    radius: 2
    color: "#08080a"
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.05)

    onLevelChanged: if (level > peak) peak = level
    Component.onCompleted: peak = level

    Timer {
        interval: 1500
        running: bar.peak > bar.level + 0.001
        onTriggered: bar.peak = Math.max(bar.level, bar.peak - 0.06)
        repeat: true
    }

    Column {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 1.2
        Repeater {
            model: bar.segments
            Rectangle {
                required property int index
                readonly property int idx: bar.segments - 1 - index
                readonly property real frac: (idx + 0.5) / bar.segments
                readonly property bool lit: bar.level >= idx / bar.segments && bar.level > 0
                width: bar.width - 2
                height: (bar.height - 2 - (bar.segments - 1) * 1.2) / bar.segments
                radius: 1
                color: idx === bar.peakIndex && bar.peak > 0.01 ? Qt.rgba(1, 1, 1, 0.95)
                     : frac >= 0.9 ? (lit ? "#ff5a4d" : Qt.rgba(1, 0.353, 0.302, 0.12))
                     : frac >= 0.72 ? (lit ? "#f0c850" : Qt.rgba(0.941, 0.784, 0.314, 0.1))
                     : (lit ? "#42c97c" : Qt.rgba(0.259, 0.788, 0.486, 0.09))
            }
        }
    }
}
