# Zephyr native UI

Qt 6, Qt Quick/QML and C++ implement the desktop UI. The React app in `ui/` is a design reference. The default app runs the Midnight fixture. An optional libardour build opens real sessions through the Dummy backend and connects mixer and transport controls. OpenRouter supplies structured AI plans. Hardware audio and native plugin editors remain separate work.

## Run and check

Requires Qt 6.11 with Quick, Quick Controls, QML and Qt Test, plus `qmake6`, a C++17 compiler and `make`.

From the repository root:

```sh
sh native/run.sh
sh native/run.sh --self-test
native/.build/zephyr-native --test-ui
native/.build/zephyr-native --view mix
native/.build/zephyr-native --view split --width 1100 --height 640
native/.build/zephyr-native --screenshot /absolute/path/arrange.png
```

GUI automation on this desktop runs through `agent-ws`. Keep interaction checks in a separate private display from any app being inspected.

```sh
agent-ws run zephyr-checks --wait -- "$PWD/native/.build/zephyr-native" --test-ui
agent-ws down zephyr-checks
```

The state checks cover validation, independent launcher sources, gesture undo, processor bypass synchronization, AI conflict rejection and atomic apply. The QML checks exercise real mouse dragging, keyboard controls, shared selection, resizing and the command palette. Use the normal Qt scene graph renderer for visual checks. Qt's software renderer does not draw the custom waveform geometry.

## Engine shell proof

The optional build links the existing libraries in `build/libs`. It uses the Dummy backend at the session sample rate, with a 1024-sample buffer. Dummy advances the real engine clock without audio device output.

This checkout has a local dependency bundle under ignored `native/.work/engine-deps/usr`. It contains Boost headers and compatible libsigc++, glibmm, aubio, liblo, liblrdf and Vamp libraries extracted from official Arch packages. Package hashes match the local pacman repository database. No system packages were installed. The engine build requires this bundle and the existing Ardour build; `run-engine.sh` reports a missing prerequisite rather than downloading packages. Set `ZEPHYR_ENGINE_ROOT` when linking a different checkout's Ardour build.

```sh
sh native/run-engine.sh --engine-self-test
agent-ws run zephyr-engine --wait -- sh "$PWD/native/run-engine.sh" \
  --new-engine-session "$PWD/native/.work/EngineProof" --session-name EngineProof
agent-ws down zephyr-engine
```

New sessions contain a real stereo master and one mono audio track. Creation rejects folders containing Ardour snapshots. Open an existing snapshot with `--engine-session /absolute/session/folder --session-name SnapshotName`. The launcher isolates Ardour configuration under `native/.work/engine-config`.

The shell projects engine route IDs, names, gain, mute, solo, mono pan and peak meters. Transport displays engine samples converted through the tempo map. Native fader and mute input changes engine controls; the shell reads back their values. Manual control edits reject automation modes other than Off. The bridge can save and reopen snapshots, although the shell does not yet provide a Save workflow.

Engine mode supplies empty clip, device and browser models. The command executor
supports independent gain, mute, solo and mono pan changes with authoritative
Preview, Apply, Undo and diffs. It rejects unsupported plugin, send, routing and
region actions before mutation. Manual and AI edits share this boundary. The
master shows unavailable loudness measurements as `--`. Clip capture, placement
and note editing currently operate on the fixture project only.

Run the native input check with a new disposable folder. The check changes its real track's gain and mute and starts transport.

```sh
agent-ws run zephyr-engine-checks --wait -- sh "$PWD/native/run-engine.sh" \
  --new-engine-session "$PWD/native/.work/EngineInputProof" \
  --session-name EngineInputProof --test-engine-ui
agent-ws down zephyr-engine-checks
node native/contracts/check.cjs
```

The bridge polls on the Qt thread every 30 ms. Qt publication occurs after releasing the engine process lock. Its limited callback queue handles null-target SessionEvent returns and rejects callbacks containing raw targets. Hardware backends require an event loop with Ardour's per-thread queues before production use.

## Design sources

- `ui/design-ref/DAWWorkspace.dc.html` defines the arrangement, transport, browser, assistant and command palette.
- `ui/design-ref/MixConsole.dc.html` defines the mixer groups, channels, returns and master.
- `ui/design-ref/DeviceChain.dc.html` defines device cards and signal flow.
- `qml/shared/Theme.qml` holds the reference palette.

The default geometry uses a 236px browser, 150px track headers, 58px lanes, 52px per bar, a 328px inspector and a 196px detail drawer. Dividers allow resizing. The mixer pins the master while groups scroll horizontally. Search, group collapse, channel widths, inserts and sends operate on the same C++ session.

The HTML references disagree on some processor names and the bus count. Mixer insert names define the fixture's processor identity. The count reflects four actual buses. Selection uses one shared track and clip rather than the reference's two selected clips. Fixture MIDI previews use the reference's seeded note layout. Device graphs respond to fixture parameters and do not represent plugin DSP curves.

## State and commands

`Session` owns the fixture, selection and undo. QML calls its validated mutations. Arrangement clips carry timeline placements. Launcher slots reference independent `clipSources`. `Waveform` draws seeded reference geometry through the Qt scene graph.

Drag gestures create one undo step. Mixer inserts and device cards share explicit processor IDs. AI preview exposes physical before/after values without changing the project. Apply checks the entire plan before making one undoable edit. A manual edit can make a staged plan conflict. Apply then rejects the plan without changing parameters.

Ctrl/Cmd K opens the command palette. Ctrl/Cmd Z undoes an edit. Space toggles the demo transport outside text fields. Alt 1 through Alt 4 select views. Arrow keys adjust focused knobs and faders. Arrangement arrows nudge the selected clip. Ctrl plus/minus and Ctrl wheel zoom the timeline.

The application uses OpenRouter for natural-language requests. Configure
`OPENROUTER_API_KEY` and `OPENROUTER_MODEL` before starting it. The key never enters
project state. [AI command planning](../docs/AI_COMMANDS.md) documents the request
context, validation, supported operations and provider limits. Check modes use
local rules and the built-in vocal cleanup and chorus width examples. Unsupported
requests show a notice. Reference history is labeled separately from edits made in the app. Transport advances the visual playhead only. Meters and CPU values are reference data. Record, metronome, monitor controls, track creation and new send routing require engine integration. Empty launcher slots create a MIDI source with Return or a double-click. The
piano roll adds notes through its Add note button or a quantized grid double-click.
Place in arrangement copies the source into an independent timeline region.
These fixture edits support undo; they do not record or schedule engine audio.

Mix history retains 64 transactions in memory. Named snapshots capture mixer
state and restore it in one undo step without changing clips or selection.
Engine snapshots restore supported route controls through one validated command
batch. History and snapshots do not persist across restarts. Production sessions
need persistence and measured rendering budgets. Native plugin editor hosting remains a separate validation task.
