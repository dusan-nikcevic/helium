# Zephyr Product Direction

Zephyr is an AI-native DAW with a pro audio engine.

The product should not feel like a prettier Ardour. It should feel like a new
DAW built on a serious engine: faster than traditional studio tools, calmer than
beatmaking tools, and more transparent than current AI assistants.

## Product Formula

Build a hybrid of the best current DAW ideas:

```txt
Ableton's speed
+ Logic's polish
+ FL Studio's beat workflow
+ Pro Tools/LUNA-style mixer clarity
+ Bitwig's modulation and automation depth
+ Studio One's drag-and-drop production flow
+ Cursor-like AI command and diff layer
```

For the Ardour-engine path, the design target is:

```txt
Cleaner than Ableton.
Friendlier than Pro Tools.
Prettier than Ardour.
Less chaotic than FL Studio.
More AI-native than all of them.
```

## Positioning

Primary positioning:

```txt
An AI-native DAW with a pro audio engine.
```

Secondary positioning:

```txt
Cursor for music production.
```

Avoid positioning Zephyr as "a prettier Ardour." Ardour is the engine
foundation, not the product story.

## What To Borrow

| DAW | Borrow | Avoid |
| --- | --- | --- |
| Ableton Live | Single-window speed, browser-first workflow, Session View plus Arrangement View, bottom detail panel | Mixer that feels secondary |
| Logic Pro | Polished defaults, approachable UX, smart assistant-style production tools | Mac-only assumptions and legacy menu depth |
| FL Studio | Channel Rack, Step Sequencer, Piano Roll speed | Floating-window chaos and unclear track/instrument/mixer mapping |
| Bitwig | Modulation-first design, clip launcher, automation clips, scale/key awareness | Overly modular feel for normal musicians |
| Studio One | Drag/drop workflow, integrated launcher, end-to-end production and mastering mindset | Bland visual identity |
| Pro Tools | Professional edit/mix mental model, routing, automation credibility | Intimidating old-school UX |
| LUNA | Beautiful console-like mixer and simple Timeline/Mixer split | Hardware-tied analog assumptions |
| Reason | Track-centric access to devices, signal chain, levels, and sends | Busy rack metaphor |

## Workspace Model

Use one focused workspace. Avoid random floating windows except plugin UIs.

```txt
Top:     Transport, tempo, key, scale, CPU, project state
Left:    Browser, samples, instruments, plugins, AI history
Center:  Timeline, Arrangement, Clip Launcher
Bottom:  Clip editor, Piano Roll, audio editor, device chain
Right:   Track inspector, AI command panel, properties
```

The UI should make the main task obvious at all times:

- Browse sounds and devices.
- Capture ideas in clips.
- Arrange the song.
- Edit MIDI or audio in context.
- Mix with confidence.
- Ask AI for structured changes.

## Track Model

Use a simple default rule:

```txt
One track = one musical object.
```

By default, a track owns:

- Clips
- Instrument or audio input
- Device chain
- Mixer channel
- Sends
- Automation

Advanced users can break this rule with buses, folders, sidechains, multi-output
instruments, and custom routing. Beginners should never wonder where sound is
coming from.

## Arrangement Workflow

Copy Ableton and Bitwig more than Logic for composition flow:

```txt
Clip Launcher for ideas
-> Arrangement Timeline for song structure
-> Detail Editor for MIDI/audio
-> AI command palette over all of it
```

This dual workflow should exist early. The launcher creates ideas quickly; the
timeline turns them into songs.

## Beatmaking Workflow

Borrow FL Studio's speed, not its window model.

Zephyr should have a Channel Rack-style beat surface that is integrated with the
track model:

- Each rack row maps clearly to a track, drum lane, or instrument part.
- Step sequencing and Piano Roll editing are one click apart.
- Pattern clips appear in the launcher and arrangement as normal musical clips.
- Mixer routing remains visible and predictable.

## Mixer Mode

The mixer should be a serious first-class mode inspired by Pro Tools, LUNA, and
Cubase.

```txt
Mix View:
- Vertical channel strips
- Always-readable inserts, sends, routing, pan, fader, and meter
- Clear track folders, buses, VCAs, and groups
- Searchable channels
- Mix snapshots
- Undo history for mixer changes
- AI change diff panel
```

The mixer must feel beautiful, but more importantly it must feel trustworthy.
When AI changes a mix, users must see the exact changes before accepting them.

Example:

```txt
AI request:
"Clean up the vocal chain and balance the chorus."

Changed:
- Lead Vocal: EQ gain at 250 Hz changed by -2.0 dB
- Lead Vocal: compressor threshold changed from -18 dB to -21 dB
- Vocal Bus: Plate Reverb send increased by 8%
- Chorus Guitars: stereo width increased by 15%

Actions:
Preview / Apply / Undo
```

This is the music-production version of Cursor's code diff.

## AI Command Layer

AI is core UX, not a sidebar helper.

The core loop:

```txt
Select anything -> describe edit -> preview -> apply -> undo
```

Example commands:

```txt
"Clone this MIDI to a new warm pad and play diatonic thirds above."
"Make a bassline from these chords."
"Make this drum pattern less robotic."
"Turn this 8-bar loop into intro, verse, and chorus."
"Clean up this vocal chain but keep it natural."
"Make the chorus wider without making it louder."
"Route all drums to a drum bus and add gentle glue compression."
```

AI should emit structured DAW commands instead of arbitrary scripts:

```json
{
  "action": "create_harmony_track",
  "source": "selected_midi_clip",
  "harmony": "diatonic_thirds_above",
  "instrument": "warm_pad",
  "velocity_scale": 0.75,
  "preview": true
}
```

Every AI action should support:

- Preview before commit
- Human-readable diff
- Apply
- Undo
- History
- Parameter-level inspection for mix and routing changes

## Beginner And Pro Modes

Beginner mode should optimize for clear defaults:

- One track equals one musical object.
- Templates guide common workflows.
- AI explains available actions through commands, not tutorials.
- Routing stays simple unless the user asks for advanced control.

Pro mode should expose the full DAW:

- Full routing
- Buses, folders, VCAs, and groups
- Automation lanes and automation clips
- Plugin chains
- Scripting and command automation
- Detailed AI diffs

## Technical Shape

Recommended architecture:

```txt
Engine:
Ardour/libardour

UI:
Modern custom shell, likely React/Tauri or native GPU UI

Workflow:
Ableton/Bitwig-style arrangement plus launcher

Beatmaking:
FL-style Channel Rack integrated into the track model

Mixer:
LUNA/Pro Tools-inspired console with Cubase-style undo/history

AI:
Cursor-style command palette with preview/diff/apply/undo
```

For the current Ardour fork, this implies a hard boundary:

- Keep Ardour/libardour as the audio/session engine.
- Build a new product shell on top.
- Avoid inheriting Ardour's legacy UI assumptions.
- Expose engine operations through structured commands that AI can plan,
  preview, apply, and undo.

## First Viral Demo

The first demo should be simple and concrete:

```txt
1. Open project.
2. Select MIDI melody.
3. Type: "clone this to a new synth and play thirds."
4. New track appears.
5. User hears before/after.
6. AI diff shows exactly what changed.
7. User applies or undoes the change.
```

This is the wedge. The UI attracts attention; the AI edit workflow gives users a
reason to stay.

## Source References

- Ableton Live Concepts: https://www.ableton.com/en/live-manual/12/live-concepts/
- FL Studio workflow: https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/basics_workflow.htm
- Bitwig: https://www.bitwig.com/
- Studio One Pro 7 features: https://splice.com/blog/top-features-presonus-studio-one/
- LUNA mixer concepts: https://help.uaudio.com/hc/en-us/articles/360041440592-Using-LUNA
- Cubase MixConsole history: https://www.steinberg.help/r/NlutMEjaYjAqk1bhdcAc5g/zoUjsofxpOR5qEcQ~6Y0XQ
- SoundFlow Session Assistant: https://soundflow.org/session-assistant
- FL Studio AI assistant coverage: https://www.musicradar.com/music-tech/fl-studio-2025-introduces-chatgpt-style-ai-assistant-that-offers-music-production-advice-and-helps-you-use-the-daw
