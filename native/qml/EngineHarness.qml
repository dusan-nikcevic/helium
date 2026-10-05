import QtQuick
import QtTest
import Zephyr 1.0

Main {
    id: proof
    TestCase { id: input; name: "EngineInput"; when: false }
    property int checkCount: 0
    property int step: 0
    property string trackId: ""
    property real originalGain: 0
    property real keyboardGain: 0
    property real transportStart: 0
    property real transportStopped: 0
    property var controls: ({})
    function assertThat(condition, message) {
        checkCount++
        if (!condition) throw new Error(message)
    }
    function findNamed(item, name) {
        if (item.objectName === name) return item
        for (const child of item.children || []) {
            const found = findNamed(child, name)
            if (found) return found
        }
        return null
    }
    function runTests() { requestActivate(); clock.start() }
    // Return to the event loop between requests and asynchronous engine readback.
    Timer {
        id: clock
        interval: 250
        repeat: true
        onTriggered: {
            try {
                switch (proof.step++) {
                case 0: {
                    proof.assertThat(Session.engineConnected && Session.view === "mix", "Shell uses engine mode")
                    proof.assertThat(Session.project.tracks.length === 1, "Shell projects the new engine track")
                    proof.assertThat(Session.project.master.engineId.length > 0, "Master alias resolves the real engine route")
                    proof.assertThat(Session.stagedAiPlan.length === 0 && Session.aiHistory.length === 0, "Engine mode excludes fixture AI data")
                    proof.trackId = Session.project.tracks[0].id
                    const fader = proof.findNamed(contentItem, "fader-" + proof.trackId)
                    proof.assertThat(fader !== null, "Real engine track renders a native fader")
                    proof.originalGain = Session.project.tracks[0].volumeDb
                    fader.forceActiveFocus()
                    input.keyClick(Qt.Key_Down)
                    break
                }
                case 1: {
                    proof.assertThat(Session.project.tracks[0].volumeDb < proof.originalGain, "Keyboard fader edit changes engine gain")
                    const fader = proof.findNamed(contentItem, "fader-" + proof.trackId)
                    const area = proof.findNamed(fader, "fader-input")
                    const thumbY = area.height * (1 - fader.norm)
                    proof.keyboardGain = Session.project.tracks[0].volumeDb
                    input.mousePress(area, area.width / 2, thumbY)
                    input.mouseMove(area, area.width / 2, thumbY + 40)
                    input.mouseRelease(area, area.width / 2, thumbY + 40)
                    break
                }
                case 2:
                    proof.assertThat(Session.project.tracks[0].volumeDb < proof.keyboardGain, "Pointer fader edit changes engine gain")
                    input.mouseClick(proof.findNamed(contentItem, "switch-" + proof.trackId + "-muted"))
                    break
                case 3:
                    proof.assertThat(Session.project.tracks[0].muted, "Mute button changes the engine track")
                    proof.assertThat(Session.setTrackValue(proof.trackId, "volumeDb", proof.originalGain), "Engine gain can restore its prior value")
                    proof.assertThat(Session.setTrackValue(proof.trackId, "muted", false), "Engine mute can restore its prior value")
                    break
                case 4:
                    proof.assertThat(Math.abs(Session.project.tracks[0].volumeDb - proof.originalGain) < .001 && !Session.project.tracks[0].muted, "Restored engine controls read back")
                    proof.controls = Session.project.tracks[0]
                    proof.assertThat(!Session.applyAi() && !Session.undo() && !Session.addDevice(proof.trackId, "eq"), "Unconnected engine actions reject")
                    break
                case 5:
                    proof.assertThat(Session.project.tracks[0].volumeDb === proof.controls.volumeDb && Session.project.tracks[0].muted === proof.controls.muted, "Rejected actions preserve engine controls")
                    proof.transportStart = Session.playheadBar
                    input.mouseClick(proof.findNamed(contentItem, "transport-play"))
                    break
                case 6:
                    proof.assertThat(Session.playing && Session.playheadBar > proof.transportStart, "Native play button starts engine transport")
                    input.mouseClick(proof.findNamed(contentItem, "transport-play"))
                    break
                case 7:
                    proof.transportStopped = Session.playheadBar
                    break
                case 8:
                    proof.assertThat(!Session.playing && Session.playheadBar === proof.transportStopped, "Native play button stops engine transport")
                    proof.assertThat(Session.seek(3), "Native state requests a tempo-map seek")
                    break
                case 9:
                    proof.assertThat(Math.abs(Session.playheadBar - 3) < .002, "Engine seek reaches the requested bar")
                    proof.width = 1100; proof.height = 640
                    break
                case 10:
                    proof.assertThat(mixerPanel.width >= 1000 && mixerPanel.height > 240, "Engine mixer retains native minimum geometry")
                    clock.stop()
                    console.log("Engine QML integration: " + proof.checkCount + " checks, 0 failures")
                    Qt.exit(0)
                }
            } catch (error) {
                clock.stop()
                console.error("Engine QML integration failed: " + error.message)
                Qt.exit(1)
            }
        }
    }
}
