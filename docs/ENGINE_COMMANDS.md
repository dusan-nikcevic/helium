# Engine command contract

Version 1 defines the shared envelope for manual controls and AI plans. The
machine-readable contract is in [command.schema.json](../native/contracts/command.schema.json)
and [result.schema.json](../native/contracts/result.schema.json). These schemas
use [JSON Schema 2020-12](https://json-schema.org/draft/2020-12/json-schema-core).
They define a contract, not proof that every operation has an engine adapter.
An adapter must report its supported actions and reject unsupported operations.

## Targets and values

`sessionId` is the engine session UUID. Each target contains an object kind and
its persistent engine ID. A processor, send or region also names its owning
route. Regions name their playlist. Names and array positions are display data.
See [Ardour object mapping](ENGINE_OBJECTS.md) for engine ownership.

`selection` captures resolved targets when the request starts. A later selection
change cannot redirect a staged command. An AI request such as "selected track"
must resolve to IDs before validation. Operations can target other objects when
the requested change needs them, such as a selected track's send destination.

| Action                  | Target             | Value                                                       |
| ----------------------- | ------------------ | ----------------------------------------------------------- |
| `set_route_control`     | Route              | `gainDb`, `panPosition`, `muted`, `soloed` or `recordArmed` |
| `set_parameter`         | Processor          | Descriptor value with `parameterId` and explicit `unit`     |
| `set_processor_enabled` | Processor          | Boolean                                                     |
| `set_send_gain`         | Send               | Gain in dB                                                  |
| `move_region`           | Region in playlist | Tagged position in beats or integer samples                 |

Gain values use dB. The adapter converts to Ardour gain coefficients. The v1
floor of -120 dB means zero gain on writes; reads below the floor also report
-120 dB. Pan position uses 0 through 1 with center at 0.5. The fixture's pan
control uses -1 through 1, so conversion is `(pan + 1) / 2`. A surround panner
cannot accept this action unless it exposes that same control capability.

Plugin values use the descriptor's physical value and unit, such as dB or Hz.
Use `unitless` when the descriptor has no unit. The adapter resolves
`parameterId` to its control and verifies units, bounds, step size and writability.
A normalized QML knob must use the control's conversion; fixture linear scales
do not define real plugin behavior. Enum and toggle parameters remain numeric
descriptor values, with the descriptor determining valid choices.

Musical positions count quarter-note beats from the project origin. Sample
positions count integer samples from the same origin at the session sample rate.
The adapter checks that `expected` and `value` have the same domain. It converts
displayed bars through the tempo and meter map. It never applies the fixture's
four-beat, constant-tempo formula to an engine session. Moving a region changes
its placement, not its media offset or length.

`groupMode` is `independent` in v1. The adapter uses the engine's no-group
disposition. Linked controls and clip creation require explicit contract extensions.
Routing and MIDI transpose use the explicit alternatives documented in
[AI command planning](AI_COMMANDS.md). Adapters reject alternatives they cannot
execute and restore. A planner cannot smuggle them into a parameter batch.

## Preview, apply and undo

Every request includes a `commandId`, source `ui` or `ai`, and
`expectedRevision`. The engine adapter owns a monotonic revision for durable
edits. Selection, playback, meters and preview do not advance it. Engine edits
made outside the command API must invalidate pending plans and advance the
bridge revision when observed.

1. **Preview** validates the entire batch against one revision. It resolves
   ownership and capabilities, compares every `expected` value, and computes
   diffs without changing the live session or its undo history. Audio audition
   requires a separate isolated render; this contract's preview is a state diff.
2. **Apply** revalidates the entire batch. A stale revision or mismatched prior
   value rejects the request before mutation. The adapter serializes durable
   edits on its control thread and creates one reversible transaction.
3. **Undo** names the returned `transactionId` and current expected revision.
   Version 1 allows undo of the applied transaction at the head of history only.
   A later edit causes `UNDO_CONFLICT`; there is no silent selective undo.
   The result reports the actual inverse diff and the new revision.

Each operation contains its expected prior value. Duplicate writes to the same
target and property in one batch are invalid. IDs, capabilities, units, ranges,
automation mode, media availability and rollback support require runtime checks
beyond JSON Schema. Floating values use the control descriptor's precision for
comparison. Reject non-finite numbers before JSON serialization and validation.

The adapter plans the complete batch before mutation. It captures inverse
operations or state undo commands before writing. If an engine mutation fails,
the adapter restores every already-written value before returning a rejection.
`abort_reversible_command()` only clears collected history commands; it does
not roll back the session. An operation without a proven rollback path reports
`UNSUPPORTED_OPERATION`. A rollback failure stops further edits and reports
`ENGINE_FAILURE`; the UI must reload authoritative state rather than claim that
the project stayed unchanged.

A continuous gesture validates each live update but commits one history entry
on release. An AI batch uses the same validation and mutation functions. Source
is attribution, not an authority to bypass checks.

Idempotency keys contain session ID, phase and command ID. An exact apply or undo
retry returns the stored result before checking its now-stale revision. A
different payload with the same key reports `IDEMPOTENCY_CONFLICT`. A preview
retry revalidates against the current revision and never reuses stale diffs.
The adapter retains mutation receipts with session history for the active
session. A process restart invalidates receipts unless an implementation stores
them durably; v1 does not promise replay across restarts.

## Results and failures

Success returns `previewed`, `applied` or `undone`, the authoritative revision,
and parameter-level changes. Apply and undo return a transaction ID. Each diff
contains the target, property, before/after values and engine-generated display
labels. UI and AI share these result fields; model-generated labels cannot
override the measured values.

Every result includes `stateValid`. A rollback failure sets it to `false`.
Rejection returns an empty change list and an error object with a code,
human-readable message, recoverability and a JSON Pointer `path`. An operation
failure also includes its index and resolved target. Conflict errors include
`expected` and `actual` when known. A rollback failure is the exception to an
unchanged-state rejection, and requires an authoritative reload as described
above. Rejected results never enter the successful edit history.

| Code                                                       | UI behavior                                            |
| ---------------------------------------------------------- | ------------------------------------------------------ |
| `INVALID_COMMAND`, `UNSUPPORTED_VERSION`                   | Show the contract error and reject the payload         |
| `UNKNOWN_SESSION`, `TARGET_NOT_FOUND`, `CONTROL_NOT_FOUND` | Refresh selection and available controls               |
| `VALUE_OUT_OF_RANGE`                                       | Show the descriptor range beside the control           |
| `UNSUPPORTED_OPERATION`                                    | Show that the active adapter lacks this operation      |
| `CONFLICT`, `AUTOMATION_CONFLICT`                          | Re-read state and rebuild the preview                  |
| `UNDO_CONFLICT`                                            | Show that a later edit must be undone first            |
| `IDEMPOTENCY_CONFLICT`                                     | Stop retrying that command ID                          |
| `ENGINE_UNAVAILABLE`                                       | Disable engine edits and show connection state         |
| `ENGINE_FAILURE`                                           | Reload authoritative state and show the engine failure |

## Validate the contract

```sh
node native/contracts/check.cjs
```

The check requires Ajv 8. If it is absent, install a local validator with
`npm install --prefix native/contracts --no-save --package-lock=false ajv@8.17.1`.
It validates request/result examples and rejects malformed
targets, illegal phase combinations, bad value types and unexpected fields.
`native/contracts/examples.json` contains UI and AI requests plus preview,
apply, undo and conflict results. These are contract examples, not prerecorded
engine receipts.
