# Ardour object mapping

The [product glossary](../GLOSSARY.md) defines musical objects. This document maps
those objects to the current Ardour fork and the native Qt model.
The [command contract](ENGINE_COMMANDS.md) uses these identities and ownership rules.

## Engine and UI objects

| Zephyr object        | Ardour equivalent                                               | Default                                                                                                                       |
| -------------------- | --------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| Project              | `ARDOUR::Session` plus Zephyr workspace state                   | Engine owns session, media, routing, transport and history. Zephyr owns view, selection, AI plans and panel geometry.         |
| Mixer channel        | `Stripable`, normally `Route`                                   | Route owns I/O, controls and processors. A VCA is a stripable without route I/O.                                              |
| Audio track          | `AudioTrack`, derived from `Track`                              | Track adds playlists and recording to a route.                                                                                |
| MIDI track           | `MidiTrack`                                                     | MIDI playlist holds its regions.                                                                                              |
| Instrument track     | MIDI track with instrument `PluginInsert`                       | Instrument is a track role, not another Ardour track class. Preserve explicit user classification when plugins change.        |
| Bus                  | Non-track `Route`                                               | No recording playlist. Preserve audio or MIDI capabilities.                                                                   |
| FX return channel    | Bus with Zephyr return role                                     | A return channel receives sends. `ARDOUR::Return` is an input processor. `InternalReturn` sums internal sends inside a route. |
| Master               | `Session::master_out()`                                         | Resolve the engine master ID. A session can lack a master. Monitor output is separate.                                        |
| Arrangement clip     | `Region` in active `Playlist`                                   | Region position is timeline placement; start is media offset; length is playback extent.                                      |
| Clip source          | Reusable region description backed by media `Source` references | A Zephyr clip source includes boundaries. Ardour Source holds underlying media. A region can reference multiple sources.      |
| Launcher slot        | `Trigger` in a route's `TriggerBox`                             | Track and row locate the slot; trigger ID identifies the current engine instance. Its region need not belong to a playlist.   |
| Scene launch         | `Session::trigger_cue_row(row)`                                 | Queues a cue and requests transport. A returned call does not prove that clips started.                                       |
| Device and insert    | Two UI projections of one `PluginInsert`                        | Share processor identity and enabled state. Other processors need explicit capability checks.                                 |
| Plugin catalog entry | `PluginInfo`, format and `unique_id`                            | Catalog identity differs from a loaded processor instance. Multiple inserts can load one plugin.                              |
| Device parameter     | `Evoral::Parameter`, `AutomationControl`, `ParameterDescriptor` | Control identity addresses the parameter; descriptor defines range, unit, steps and normalized conversion.                    |
| Send                 | `Send`, normally `InternalSend` for another route               | Preserve target route and processor position for pre/post-fader behavior. External sends use delivery I/O.                    |
| Output routing       | Route output I/O connections                                    | `busId` is a simple projection. Routing labels summarize authoritative port connections.                                      |
| Channel volume       | `GainControl`                                                   | Convert displayed dB to a gain coefficient.                                                                                   |
| Pan                  | `Pannable` and panner controls                                  | Simple stereo position is one capability. Preserve channel layout and panner configuration for advanced routes.               |
| Mute and solo        | `MuteControl`, `SoloControl`                                    | Separate own switch state from effective state caused by routing, groups and control masters.                                 |
| Record arm           | `Track::rec_enable_control()`                                   | Require a recordable track. A generic route cannot record.                                                                    |
| Folder               | Zephyr visual organization                                      | Independent of a route group, bus or VCA assignment.                                                                          |
| Route group          | `RouteGroup`                                                    | Selected linked controls and optional subgroup routing. Visual grouping alone does not link controls.                         |
| VCA                  | `VCA`                                                           | Gain, mute and solo controls without route I/O, playlist or plugins.                                                          |
| Automation lane      | Control's `AutomationList`                                      | Automation mode and touch state govern writes.                                                                                |
| Mix snapshot         | `MixerScene`                                                    | Different from a launcher scene and an engine session snapshot.                                                               |

## Identity and ownership

Use the session UUID, object kind and engine object's `PBD::ID`. Names, strip
order and array indices never identify command targets. Rename and reorder
operations preserve IDs. The fixture's `master` ID is only a display alias.

A region target includes its playlist and track. A processor or send target
includes its route. Verify ownership before applying a command. A plugin
parameter also needs its control identity. Validate plugin instance indices;
`PluginInsert::plugin()` falls back to plugin zero for an out-of-range index.

A launcher slot has a stable Zephyr identity plus its route, row and current
trigger association. Loading content can replace the trigger. Reusable clip
content, arrangement placement, launcher slot and media source keep separate IDs.

Give each arrangement placement its own region metadata. Share the underlying
media sources. Launcher regions are independent by default. Copying a timeline
clip into a slot must not make later timeline trimming change the launcher clip.
`TriggerBox::set_from_selection()` does not clone an ordinary region.
`Trigger::set_region_internal()` clones whole-file regions but shares other
regions, so Zephyr must explicitly clone metadata when independence is required.

In the fixture, `slot.clipId` addresses `clipSources` when that collection exists.
The fallback to arrangement clip IDs only supports old fixture data. Engine
integration must not use that fallback as an ownership rule.

## Launcher defaults

Use one-bar scene quantization by default. Preserve per-slot launch style and
quantization. An empty scene slot requests a quantized stop on its track.
Cue-isolated slots ignore scene launch. These rules match the engine's cue flow.

Publish scheduled and playing states separately. `trigger_cue_row()` schedules
work. `TriggerBox::active_scene()` is only valid inside the processing call tree;
the GUI must consume safe notifications or a bridge snapshot instead.

## Values and time

Read controls and descriptor values from the engine. Send gain is not inherently
a percentage. Define a UI conversion for the selected send control and convert
back before applying. The fixture's percent send knob is a visual contract only.

Plugin normalized conversion follows the control descriptor. Do not reuse the
fixture's linear `min`/`max` display formatting for logarithmic plugin controls.
Parameter writes must respect `AutomationControl::writable()`, automation mode,
touch state and group disposition. A raw plugin setter bypasses those controls.

Keep timeline position, media offset and duration separate. Preserve `timepos_t`
and `timecnt_t` domains. Convert bar labels through the tempo and meter map.
Sample positions and musical positions need explicit conversion; the fixture's
constant-tempo timer is not an engine clock.

## Threading and undo

Publish Qt model changes on the GUI thread. Audio callbacks never call QML or
wait for the GUI. Follow each operation's thread contract. `Trigger::set_region()`
accepts GUI requests and delegates loading to a worker. `TriggerBox::set_region()`
runs on that worker.

Manual and AI edits use the same validated engine operations. Group one gesture
or AI batch into one reversible command. Opening a reversible command does not
make arbitrary mutations undoable; collect state or property undo commands.
`abort_reversible_command()` clears collected history without reverting changes.
A failed batch needs explicit rollback.

The native fixture's 64 snapshots and staged parameter diffs prove UI behavior.
They do not prove engine undo, audio audition, plugin DSP or persistence.

## Source anchors

- [Session identity, routes, master and mixer scenes](../libs/ardour/ardour/session.h)
- [Routes and their controls](../libs/ardour/ardour/route.h)
- [Track playlists and recording](../libs/ardour/ardour/track.h)
- [Regions and media references](../libs/ardour/ardour/region.h)
- [Playlists](../libs/ardour/ardour/playlist.h)
- [Trigger ownership and processing state](../libs/ardour/ardour/triggerbox.h)
- [Region assignment and empty-slot cue handling](../libs/ardour/triggerbox.cc)
- [Scene scheduling](../libs/ardour/session_process.cc)
- [Plugin instances and their controls](../libs/ardour/ardour/plugin_insert.h)
- [Plugin catalog identity](../libs/ardour/ardour/plugin.h)
- [Parameter descriptors](../libs/ardour/ardour/parameter_descriptor.h)
- [Automation writability](../libs/ardour/ardour/automation_control.h)
- [Internal sends](../libs/ardour/ardour/internal_send.h)
- [Return processor](../libs/ardour/ardour/return.h)
- [Internal return processor](../libs/ardour/ardour/internal_return.h)
- [Route group controls](../libs/ardour/ardour/route_group.h)
- [VCA capabilities](../libs/ardour/ardour/vca.h)
- [History and abort behavior](../libs/pbd/history_owner.cc)
