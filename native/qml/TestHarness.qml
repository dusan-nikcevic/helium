import QtQuick
import QtTest
import Zephyr 1.0

Main {
    TestCase { id: input; name: "NativeInput"; when: false }
    property int checkCount: 0
    function assertThat(condition, message) {
        checkCount++
        if (!condition) throw new Error(message)
    }
    function runTests() {
        try {
            function findNamed(item, name) {
                if (item.objectName === name) return item
                for (const child of item.children || []) {
                    const found = findNamed(child, name)
                    if (found) return found
                }
                return null
            }
            const topBar = findNamed(contentItem, "top-bar")
            assertThat(topBar.children[1].children.every(child => child.height <= topBar.height), "Transport controls fit the top bar")
            requestActivate()
            input.wait(100)
            const browser = findNamed(contentItem, "library-browser")
            const browserWidth = browser.width
            input.mousePress(contentItem, browserWidth + 1, 400)
            input.mouseMove(contentItem, browserWidth + 51, 400)
            input.mouseRelease(contentItem, browserWidth + 51, 400)
            input.wait(50)
            assertThat(browser.width > browserWidth, "Browser divider resizes through real input")
            input.mouseClick(findNamed(contentItem, "view-mix"))
            assertThat(Session.view === "mix", "Mix button changes views through real input")
            input.wait(50)
            input.mouseWheel(mixerPanel, 700, 65, 0, -120)
            assertThat(mixerPanel.scrollPosition > 0, "Mouse wheel scrolls mixer groups horizontally")
            mixerPanel.scrollPosition = 0
            const fader = findNamed(contentItem, "fader-lead-vocal")
            const faderInput = findNamed(fader, "fader-input")
            const volume = Session.project.tracks[2].volumeDb
            const thumbY = faderInput.height * (1 - fader.norm)
            input.mousePress(faderInput, faderInput.width / 2, thumbY)
            input.mouseMove(faderInput, faderInput.width / 2, thumbY + 30)
            input.mouseMove(faderInput, faderInput.width / 2, thumbY + 60)
            input.mouseRelease(faderInput, faderInput.width / 2, thumbY + 60)
            assertThat(Session.project.tracks[2].volumeDb < volume, "Continuous fader drag changes volume")
            assertThat(Session.undo() && Session.project.tracks[2].volumeDb === volume, "One undo restores a continuous fader drag")
            input.mouseClick(findNamed(contentItem, "view-arrange"))
            input.wait(50)
            const clip = findNamed(contentItem, "clip-lead-vocal-clip-2")
            const start = Session.project.tracks[2].clips[2].startBar
            input.mousePress(clip, 50, 25)
            input.mouseMove(clip, 80, 25)
            input.mouseMove(clip, 100, 25)
            input.mouseRelease(clip, 100, 25)
            assertThat(Session.project.tracks[2].clips[2].startBar > start, "Clip drag survives shared selection updates")
            assertThat(Session.undo() && Session.project.tracks[2].clips[2].startBar === start, "One undo restores the dragged clip")
            assertThat(arrangementPanel.tracks.length === 8, "Arrangement has eight tracks")
            assertThat(mixerPanel.groups.length === 4, "Mixer has four groups")
            assertThat(mixerPanel.trackById("plate-verb") !== null, "FX returns appear in the mixer")
            Session.setView("session")
            assertThat(Session.view === "session", "Session view changes shared state")
            const scene = Session.project.launcherScenes[0]
            const slot = scene.slots.find(slot => slot.clipId)
            assertThat(Session.selectLauncherSlot(scene.id, slot.trackId), "Launcher slot selects a clip")
            assertThat(detailVisible && detailMode === "clip", "Clip selection opens the detail editor")
            assertThat(Session.selectedClipId === slot.clipId, "Launcher selection reaches the editor")
            openDevices("lead-vocal")
            assertThat(detailMode === "devices" && Session.selectedTrackId === "lead-vocal", "Device drawer shares track selection")
            input.wait(50)
            const knob = findNamed(contentItem, "device-knob-lead-vocal-channel-eq-250hz")
            const parameter = Session.selectedTrack.devices[0].params[0].value
            input.mousePress(knob, 13, 13)
            input.mouseMove(knob, 13, -7)
            input.mouseMove(knob, 13, -27)
            input.mouseRelease(knob, 13, -27)
            assertThat(Session.selectedTrack.devices[0].params[0].value > parameter, "Device knob drag changes the shared parameter")
            assertThat(Session.undo() && Session.selectedTrack.devices[0].params[0].value === parameter, "One undo restores a device knob drag")
            input.keyClick(Qt.Key_Up)
            assertThat(Session.selectedTrack.devices[0].params[0].value > parameter, "Device knobs support arrow keys")
            Session.undo()
            const muted = Session.selectedTrack.muted
            assertThat(Session.setTrackValue("lead-vocal", "muted", !muted), "Track mute writes to Session")
            assertThat(mixerPanel.trackById("lead-vocal").muted === !muted, "Mixer reads the same mute state")
            assertThat(Session.undo(), "Mute edit supports undo")
            assertThat(Session.selectedTrack.muted === muted, "Undo restores mute")
            assertThat(Session.submitAi("Clean up the vocal chain but keep it natural"), "Demo command stages a plan")
            const before = JSON.stringify(Session.project.tracks)
            assertThat(Session.previewAi(), "Preview stages the diff")
            assertThat(JSON.stringify(Session.project.tracks) === before, "Preview preserves track parameters")
            assertThat(Session.applyAi(), "Apply updates the demo project")
            assertThat(JSON.stringify(Session.project.tracks) !== before, "Apply changes track parameters")
            assertThat(Session.undo(), "AI apply supports undo")
            assertThat(JSON.stringify(Session.project.tracks) === before, "AI undo restores exact parameters")
            mixerPanel.query = "vocal"
            assertThat(mixerPanel.matches("Lead Vocal", {name: "Vocals"}), "Mixer search includes matching channels")
            assertThat(!mixerPanel.matches("Drums", {name: "Drums"}), "Mixer search excludes unrelated channels")
            mixerPanel.toggleGroup(mixerPanel.groups[1].id)
            assertThat(mixerPanel.collapsed[mixerPanel.groups[1].id], "Mixer groups collapse")
            commandPalette.open()
            assertThat(commandPalette.visible, "Command palette opens")
            commandPalette.close()
            requestActivate()
            input.wait(100)
            input.keyClick(Qt.Key_K, Qt.ControlModifier)
            input.wait(50)
            assertThat(commandPalette.visible, "Ctrl K opens the command palette")
            input.keyClick(Qt.Key_Escape)
            input.wait(50)
            assertThat(!commandPalette.visible, "Escape dismisses the command palette")
            width = 1100; height = 640
            input.wait(50)
            assertThat(findNamed(contentItem, "library-browser").width >= 180, "Browser retains its minimum at 1100 pixels")
            assertThat(findNamed(contentItem, "inspector-panel").width >= 270, "Inspector retains its minimum at 1100 pixels")
            assertThat(findNamed(contentItem, "detail-panel").height >= 170, "Detail editor fits at 640 pixels")
            width = 1440; height = 900
            Session.setView("session")
            input.wait(80)
            const emptySlot = findNamed(contentItem, "launcher-slot-break-piano")
            assertThat(emptySlot !== null, "Empty launcher slot has an input target")
            emptySlot.forceActiveFocus()
            input.keyClick(Qt.Key_Return)
            input.wait(80)
            const sourceId = Session.selectedClipId
            assertThat(Session.selectionOrigin === "launcher" && Session.selectedClip.type === "midi"
                       && Session.selectedClip.notes.length === 0, "Return captures a new empty MIDI source")
            input.mouseClick(findNamed(contentItem, "add-midi-note"))
            input.wait(50)
            assertThat(Session.selectedClip.notes.length === 1 && Session.selectedClip.notes[0].pitch === 60,
                       "Add note writes the selected launcher source")
            const grid = findNamed(contentItem, "piano-note-grid")
            input.mouseDoubleClickSequence(grid, grid.width * 0.3, grid.height * 0.4)
            input.wait(500)
            assertThat(Session.selectedClip.notes.length === 2, "Double-click adds a quantized MIDI note: " + Session.notice + "; grid " + grid.width + "x" + grid.height + ", visible " + grid.visible)
            const originalNotes = JSON.stringify(Session.selectedClip.notes)
            assertThat(Session.submitAi("transpose up 3 semitones"), "Selected MIDI source produces a structured plan")
            assertThat(Session.aiState === "preview" && JSON.stringify(Session.selectedClip.notes) === originalNotes,
                       "Authoritative preview preserves MIDI content")
            assertThat(Session.stagedAiPlan[0].property === "notes", "Transpose preview identifies note content")
            assertThat(Session.applyAi() && Session.selectedClip.notes[0].pitch === 63, "Structured transpose applies")
            assertThat(Session.undo() && JSON.stringify(Session.selectedClip.notes) === originalNotes, "Structured transpose undoes exactly")
            input.mouseClick(findNamed(contentItem, "place-launcher-clip"))
            input.wait(80)
            const placementId = Session.selectedClipId
            assertThat(Session.view === "arrange" && Session.selectionOrigin === "arrangement"
                       && placementId !== sourceId && Session.selectedClip.sourceId === sourceId,
                       "Place action creates an independent arrangement region")
            assertThat(Session.selectedClip.notes.length === 2, "Placement retains the captured notes")
            assertThat(Session.moveClip("piano", placementId, 21), "Captured placement moves on the timeline")
            const source = Session.project.clipSources.find(c => c.id === sourceId)
            assertThat(source && source.startBar === undefined && JSON.stringify(source.notes) === originalNotes,
                       "Placement movement preserves its source")
            assertThat(Session.undo() && Session.undo(), "Capture placement and movement remain undoable")
            assertThat(!Session.project.tracks.find(t => t.id === "piano").clips.some(c => c.id === placementId),
                       "Undo removes the placement without deleting its source")
            Session.selectTrack("lead-vocal")
            Session.setView("mix")
            detailVisible = false
            mixerPanel.query = ""
            input.wait(80)
            input.mouseClick(findNamed(contentItem, "mixHistoryButton"))
            input.wait(50)
            const snapshotName = findNamed(contentItem, "mixSnapshotName")
            snapshotName.text = "Before balance"
            input.mouseClick(findNamed(contentItem, "mixSnapshotSave"))
            input.wait(50)
            assertThat(Session.mixSnapshots.length === 1, "History panel saves a named snapshot")
            const snapshotId = Session.mixSnapshots[0].id
            const mixGain = Session.selectedTrack.volumeDb
            assertThat(Session.setTrackValue("lead-vocal", "volumeDb", mixGain - 2), "Manual balance edit applies")
            assertThat(Session.mixHistory[0].source === "manual" && Session.mixHistory[0].changes.length > 0,
                       "Manual change exposes physical values in history")
            input.wait(80)
            input.mouseClick(findNamed(contentItem, "restore-snapshot-" + snapshotId))
            assertThat(Session.selectedTrack.volumeDb === mixGain, "Snapshot restore returns the saved balance")
            input.mouseClick(findNamed(contentItem, "mixHistoryUndo"))
            assertThat(Session.selectedTrack.volumeDb === mixGain - 2 && Session.mixHistory[0].undone,
                       "Snapshot restore is one undo step with retained audit history")
            mixerPanel.showHistory = false
            Session.setView("split")
            detailVisible = true
            width = 1100; height = 640
            input.wait(100)
            assertThat(height >= 750 && minimumHeight === 750, "Split with detail enforces enough vertical space")
            const detail = findNamed(contentItem, "detail-panel")
            const bottom = detail.mapToItem(contentItem, 0, detail.height).y
            assertThat(bottom <= height - 24 && arrangementPanel.height >= 220 && mixerPanel.height >= 240,
                       "Split panels and detail remain inside the shell")
            console.log("Native QML integration: " + checkCount + " checks, 0 failures")
            return true
        } catch (error) {
            console.error("Native QML integration failed: " + error.message)
            return false
        }
    }
}
