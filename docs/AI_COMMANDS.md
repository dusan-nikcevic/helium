# AI command planning

Manual edits and planned edits use the [engine command contract](ENGINE_COMMANDS.md).
The application resolves selection to IDs before planning. A planner returns a
preview request. The command adapter validates that request and returns the
state diff that the UI displays before Apply. Apply revalidates the request;
Undo names the returned transaction. Discard removes the plan without changing
musical state.

## OpenRouter

The native application requests plans through OpenRouter at
`https://openrouter.ai/api/v1/chat/completions`. Start it with
`OPENROUTER_API_KEY` and `OPENROUTER_MODEL` in the process environment. The model
value is an OpenRouter model ID. Missing configuration reports an error when
submitting a command. The application does not silently substitute local rules.

The request contains the command text, the resolved selection context below,
and the engine command schema. It does not upload audio files. OpenRouter
returns a JSON preview request. Session validates that request before staging
changes; the provider response cannot apply an edit. The key stays in the
Authorization header and never appears in project state or error messages.

The client uses Qt Network, rejects redirects, limits responses to 256 KiB,
and cancels requests after 60 seconds. Failed, incomplete, or malformed
responses leave the project unchanged. The API shape follows the
[OpenRouter chat completion reference](https://openrouter.ai/docs/api/api-reference/chat/create-a-chat-completion).

Local checks use a loopback HTTP server. They prove request encoding and response
handling without an OpenRouter credential or a live model call.

## Local commands

`planAiCommand(text, context)` is a local rule planner. Its provider identifier is
`local-rules`. The check application labels this provider as local commands. These rules do
not call an LLM service.

| Command | Intent |
| --- | --- |
| `set gain to -6 dB` | Set the selected channel's gain |
| `lower gain by 3 dB` | Subtract 3 dB from the current gain |
| `raise volume by 2 dB` | Add 2 dB to the current gain |
| `pan left`, `pan right`, `pan center` | Set stereo position to 0, 1 or 0.5 |
| `mute`, `unmute`, `solo`, `unsolo` | Set the selected channel's own switch |
| `transpose up 3 semitones` | Raise each note in the selected MIDI clip |
| `transpose by -3 semitones` | Lower each note in the selected MIDI clip |
| `transpose down 12 semitones` | Lower each note by an octave |
| `route to Music Bus` | Replace the selected channel's single internal output |

Mixer commands also accept `selected track` or `selected channel`. Transpose
accepts `selected clip` or `selected notes`. Route destination names match whole
names without case sensitivity. Duplicate destination names reject. The planner
accepts one command with at most 2000 characters and one resolved selection.
It rejects combined commands and arbitrary code.

Gain must remain between -120 and +12 dB. Lower and raise take nonnegative
amounts; a signed absolute value belongs in `set gain`. Pan requires a supported
position control. Transpose requires an integer offset between -127 and +127
semitones and resulting MIDI pitches between 0 and 127. An instrument track's
classification does not prove that a MIDI clip is selected. Audio clips reject
note transposition.

## Context and result

Session supplies this context from its current authoritative state. IDs belong
to one session; names are display and destination-resolution data.

```json
{
  "sessionId": "session-1",
  "revision": 7,
  "selection": [{"kind": "route", "id": "piano"}],
  "selectedRoute": {
    "id": "piano", "name": "Piano", "role": "track",
    "gainDb": -4.5, "panPosition": 0.42,
    "muted": false, "soloed": false,
    "outputRouteId": "master"
  },
  "routes": [
    {"id": "piano", "name": "Piano", "role": "track", "outputRouteId": "master"},
    {"id": "music-bus", "name": "Music Bus", "role": "bus", "outputRouteId": "master"},
    {"id": "master", "name": "Master", "role": "master"}
  ]
}
```

A region selection additionally supplies `selectedRegion` with `target`, `name`,
`type` and `notes`. Its target includes region, route and playlist IDs and equals
the captured selection. Notes contain the full ordered vectors described below.
The fixture adapter may use `<routeId>-playlist` for its simulated playlist ID;
engine adapters resolve the actual playlist ID. The planner does not invent
playlist IDs.

A successful planner result contains `ok: true`, `provider: "local-rules"`,
`intent`, `request` and candidate `changes`. The request uses phase `preview`,
source `ai` and the captured revision. Candidate changes derive their values
from the supplied context. They are not engine execution receipts. The UI
replaces them with the authoritative command adapter's preview result. A failure
contains `ok: false`, provider and an error with `code` and `message`.

A service planner must return the same structured request rather than executable
scripts. Treat its response as untrusted input. Validate the entire envelope and
operation union before resolving targets or staging a preview. Model-generated
labels never replace values or labels in the command adapter's diff. Source `ai`
does not grant permission to skip ownership, revision or capability checks.

## Explicit operation extensions

Version 1 adds two operation alternatives. Existing alternatives retain their
value types and semantics. Adapters advertise their supported alternatives and
reject unsupported actions with `UNSUPPORTED_OPERATION`, including during
Preview. Supporting a musical or routing schema does not prove that an engine
adapter implements those edits.

### Route output

`set_route_output` targets a route. Its `expected` and `value` are objects
containing exactly one `routeId`. That ID identifies the single internal output
destination. The diff property is `outputRoute`; before and after display values
use authoritative destination names.

The adapter requires the current destination to match `expected`. The new
destination must exist in the same session and have bus, return or master
capability. Reject a master source, a route targeting itself, feedback, or
incompatible I/O. Validate the complete candidate routing graph for a batch,
including sends, sidechains and advanced connections. The planner's simple
output-chain check does not replace that adapter check. Reject external,
disconnected, multiple-destination or custom port routing when the adapter
cannot represent and restore its complete prior connections. This operation
replaces output routing; it does not add a send or create a bus.

Capture the prior connections before mutation. Apply failure restores them.
Undo restores the prior destination and connections through the same validated
command path. Routing labels alone cannot identify a destination.

### MIDI transpose

`transpose_notes` targets a region with its owning route and playlist. It
contains `semitones`, `expected` and `value`. Semitones is an integer between
-127 and 127. Expected and value are full ordered note arrays containing 1
through 4096 notes. Each note requires finite nonnegative `bar` and `beat`,
integer `pitch` between 0 and 127, and finite positive `length`. Timing fields
have an upper bound of one billion. Optional fields are `id`, MIDI `velocity`
between 0 and 127, MIDI `channel` between 0 and 15, and `previewRow` between 0
and 127. Unknown fields reject.

The note vectors retain the adapter's clip-relative timing projection. This
operation cannot change note onset or length. The adapter compares the entire
current vector with `expected` and verifies that `value` has the same length,
order and fields. Each resulting pitch must equal its prior pitch plus
`semitones`. Every other field must remain equal. The fixture's `previewRow` is
relative drawing metadata and stays unchanged by a uniform transpose.

The diff property is `notes`. Before and after contain complete vectors; display
values summarize count and pitch range. The adapter requires readable MIDI
content and preserves note identity. Shared media must not cause edits to other
arrangement placements or launcher clips. An adapter lacking independent
editable note content rejects. Apply and Undo need a proven rollback path for
the complete vector. Reject unknown media, ownership changes, stale vectors,
and pitch overflow before mutation.

The schemas enforce structure and bounded values. Runtime validation enforces
vector equality, transpose arithmetic, routing capabilities, feedback,
ownership, automation and rollback support.

## Checks

```sh
node native/contracts/check.cjs
c++ -std=c++17 -fPIC -Wall -Wextra -Werror -DAI_PLANNER_CHECK_MAIN \
  native/aiplanner.cpp native/tests/aiplannercheck.cpp \
  $(pkg-config --cflags --libs Qt6Core) -o /tmp/zephyr-aiplanner-check
/tmp/zephyr-aiplanner-check /tmp/zephyr-aiplanner-requests.json
node native/contracts/check.cjs /tmp/zephyr-aiplanner-requests.json
```

The C++ checks exercise selection ownership, malformed input, ranges, MIDI
metadata preservation, audio rejection, duplicate names and routing feedback.
The optional JSON file contains generated planner requests; the schema check
validates every request in that file. Neither check claims audio audition or
live service access.
