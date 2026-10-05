import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Zephyr 1.0
import "shared" as Shared
import "chrome" as Chrome
import "arrangement" as Arrange
import "mixer" as Mixer
import "ai" as AI

ApplicationWindow {
    id: window
    width: 1440; height: 900
    minimumWidth: 1100; minimumHeight: 640
    visible: true
    title: "Zephyr · Midnight"
    color: Shared.Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint
    font.family: Shared.Theme.fontFamily
    property bool browserVisible: true
    property bool inspectorVisible: true
    property bool detailVisible: false
    property string detailMode: "clip"
    property alias arrangementPanel: arrangement
    property alias mixerPanel: mixer
    property alias commandPalette: palette

    function openDevices(trackId) {
        Session.selectTrack(trackId)
        detailMode = "devices"
        detailVisible = true
    }
    function toggleAssistant() {
        if (Session.view === "mix" || Session.view === "split") palette.open()
        else inspectorVisible = !inspectorVisible
    }
    Connections {
        target: Session
        function onClipSelected() { window.detailMode = "clip"; window.detailVisible = true }
    }
    Shortcut { sequences: ["Ctrl+K", "Meta+K"]; onActivated: palette.open() }
    Shortcut { sequences: ["Ctrl+Z", "Meta+Z"]; onActivated: Session.undo() }
    Shortcut { sequence: "Alt+1"; onActivated: Session.setView("arrange") }
    Shortcut { sequence: "Alt+2"; onActivated: Session.setView("mix") }
    Shortcut { sequence: "Alt+3"; onActivated: Session.setView("split") }
    Shortcut { sequence: "Alt+4"; onActivated: Session.setView("session") }
    Shortcut {
        sequence: "Space"
        enabled: !palette.opened && !(window.activeFocusItem instanceof TextInput)
        onActivated: Session.togglePlayback()
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Chrome.TopBar {
            objectName: "top-bar"
            Layout.fillWidth: true
            onAssistantRequested: window.toggleAssistant()
            onCloseRequested: window.close()
            onMinimizeRequested: window.showMinimized()
            onMaximizeRequested: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
            DragHandler { target: null; onActiveChanged: if (active) window.startSystemMove() }
        }
        Chrome.ModeNav {
            Layout.fillWidth: true
            browserVisible: window.browserVisible; inspectorVisible: window.inspectorVisible
            onToggleBrowser: window.browserVisible = !window.browserVisible
            onToggleInspector: window.inspectorVisible = !window.inspectorVisible
        }
        SplitView {
            Layout.fillWidth: true; Layout.fillHeight: true; orientation: Qt.Vertical
            handle: Rectangle { implicitHeight: 4; color: SplitHandle.pressed ? Shared.Theme.accent : SplitHandle.hovered ? "#36363c" : "#141416" }
            SplitView {
                SplitView.fillHeight: true; SplitView.minimumHeight: 260
                orientation: Qt.Horizontal
                handle: Rectangle { implicitWidth: 3; color: SplitHandle.pressed ? Shared.Theme.accent : SplitHandle.hovered ? "#36363c" : Shared.Theme.rail }
                Chrome.Browser {
                    objectName: "library-browser"
                    visible: window.browserVisible && (Session.view === "arrange" || Session.view === "session")
                    SplitView.preferredWidth: 236; SplitView.minimumWidth: 180; SplitView.maximumWidth: 350
                }
                SplitView {
                    SplitView.fillWidth: true; SplitView.minimumWidth: 450
                    orientation: Qt.Vertical
                    handle: Rectangle { implicitHeight: 4; color: SplitHandle.pressed ? Shared.Theme.accent : SplitHandle.hovered ? "#36363c" : Shared.Theme.rail }
                    Item {
                        visible: Session.view !== "mix"
                        SplitView.fillHeight: true; SplitView.minimumHeight: 220
                        Arrange.Arrangement {
                            id: arrangement; objectName: "arrangement-panel"
                            anchors.fill: parent; visible: Session.view !== "session"
                            onOpenDevices: trackId => window.openDevices(trackId)
                        }
                        Arrange.SessionGrid { objectName: "session-panel"; anchors.fill: parent; visible: Session.view === "session" }
                    }
                    Mixer.MixConsole {
                        id: mixer; objectName: "mixer-panel"
                        visible: Session.view === "mix" || Session.view === "split"
                        compact: Session.view === "split"
                        SplitView.fillHeight: Session.view === "mix"
                        SplitView.preferredHeight: 320; SplitView.minimumHeight: 240
                        onOpenDevices: trackId => window.openDevices(trackId)
                        onReviewRequested: palette.open()
                    }
                }
                AI.Inspector {
                    objectName: "inspector-panel"
                    visible: window.inspectorVisible && (Session.view === "arrange" || Session.view === "session")
                    SplitView.preferredWidth: 328; SplitView.minimumWidth: 270; SplitView.maximumWidth: 430
                    onOpenDevices: trackId => window.openDevices(trackId)
                }
            }
            Item {
                objectName: "detail-panel"; visible: window.detailVisible
                SplitView.preferredHeight: 196; SplitView.minimumHeight: 170; SplitView.maximumHeight: 300
                Arrange.DetailEditor {
                    objectName: "clip-editor"; anchors.fill: parent; visible: window.detailMode === "clip"; mode: window.detailMode
                    onModeChangedByUser: mode => window.detailMode = mode
                    onCloseRequested: window.detailVisible = false
                }
                Mixer.DeviceChain { objectName: "device-chain"; anchors.fill: parent; visible: window.detailMode === "devices"; onCloseRequested: window.detailVisible = false }
            }
        }
        Rectangle {
            Layout.fillWidth: true; height: 24; color: Shared.Theme.rail
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14
                Text { text: Session.engineConnected ? "● libardour Dummy" : "● Demo session"; color: Shared.Theme.muted; font.pixelSize: 10 }
                Text { text: Session.notice; color: Shared.Theme.dim; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { text: "Ctrl K  Assistant    Space  Transport"; color: Shared.Theme.dim; font.pixelSize: 10 }
            }
        }
    }
    AI.CommandPalette { id: palette; parent: Overlay.overlay }
    Repeater {
        model: [Qt.LeftEdge, Qt.RightEdge, Qt.TopEdge, Qt.BottomEdge]
        MouseArea {
            required property int modelData
            readonly property bool vertical: modelData === Qt.LeftEdge || modelData === Qt.RightEdge
            x: modelData === Qt.RightEdge ? window.width - 4 : 0
            y: modelData === Qt.BottomEdge ? window.height - 4 : 0
            width: vertical ? 4 : window.width
            height: vertical ? window.height : 4
            cursorShape: vertical ? Qt.SizeHorCursor : Qt.SizeVerCursor
            onPressed: window.startSystemResize(modelData)
        }
    }
}
