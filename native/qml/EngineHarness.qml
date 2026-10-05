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
        if (item.objectName === name && item.visible !== false) return item
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
                    break
                case 4:
                    proof.assertThat(Math.abs(Session.project.tracks[0].volumeDb - proof.originalGain) < .001, "Restored engine gain reads back")
                    proof.assertThat(Session.setTrackValue(proof.trackId, "muted", false), "Engine mute can restore its prior value")
                    break
                case 5:
                    proof.assertThat(!Session.project.tracks[0].muted, "Restored engine mute reads back")
                    proof.assertThat(!Session.applyAi() && !Session.addDevice(proof.trackId, "eq"), "Missing AI plan and unsupported devices reject")
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
                    Session.selectTrack(proof.trackId)
                    proof.originalGain = Session.project.tracks[0].volumeDb
                    proof.assertThat(Session.submitAi("lower gain by 3 dB"), "Natural command requests an engine preview")
                    break
                case 11:
                    proof.assertThat(!Session.commandBusy && Session.aiState === "preview", "Engine preview completes asynchronously")
                    proof.assertThat(Math.abs(Session.project.tracks[0].volumeDb - proof.originalGain) < .001,
                                     "Engine preview preserves gain")
                    proof.assertThat(Session.stagedAiPlan.length === 1 && Session.stagedAiPlan[0].property === "gainDb",
                                     "Engine preview displays the actual gain diff")
                    proof.commandPalette.open()
                    break
                case 12:
                    input.mouseClick(proof.findNamed(contentItem, "ai-apply"))
                    break
                case 13:
                    proof.assertThat(Session.aiState === "applied" && Math.abs(Session.project.tracks[0].volumeDb - proof.originalGain + 3) < .001,
                                     "Apply button writes the engine plan")
                    proof.assertThat(Session.mixHistory[0].source === "ai" && Session.mixHistory[0].changes.length === 1,
                                     "Engine AI change appears in inspectable history")
                    input.mouseClick(proof.findNamed(contentItem, "ai-undo"))
                    break
                case 14:
                    proof.assertThat(Math.abs(Session.project.tracks[0].volumeDb - proof.originalGain) < .001 && Session.mixHistory[0].undone,
                                     "Undo button restores engine gain and retains the audit entry")
                    proof.commandPalette.close()
                    proof.assertThat(Session.captureMixSnapshot("Engine balance"), "Engine mixer snapshot captures current controls")
                    proof.assertThat(Session.setTrackValue(proof.trackId, "volumeDb", proof.originalGain - 2), "Engine snapshot comparison changes gain")
                    break
                case 15:
                    proof.assertThat(Session.restoreMixSnapshot(Session.mixSnapshots[0].id), "Snapshot restore requests one engine batch")
                    break
                case 16:
                    proof.assertThat(Math.abs(Session.project.tracks[0].volumeDb - proof.originalGain) < .001, "Engine snapshot restores saved gain")
                    proof.assertThat(Session.undo(), "Engine snapshot restore supports undo")
                    break
                case 17:
                    proof.assertThat(Math.abs(Session.project.tracks[0].volumeDb - proof.originalGain + 2) < .001,
                                     "Engine snapshot undo restores comparison balance")
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
