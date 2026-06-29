# Zephyr Engine Boundary (EPIC 1)

How the Zephyr UI talks to the Ardour-backed engine. The UI depends only on a
**command/event contract** (`ui/src/engine/contract.ts`) and the **`EngineClient`**
interface (`ui/src/engine/client.ts`) — never on a concrete transport. Today the
UI runs on `MockEngineClient` (the demo fixture); a real transport-backed client
swaps in with no UI changes.

## Concept mapping (E1-T2): Ardour -> Zephyr

| Ardour (`libardour`) | Zephyr object | Notes |
| --- | --- | --- |
| `Session` | `Project` | tempo map -> `bpm` / `timeSig`; sample rate -> `sampleRate` |
| `Route` (track/bus) | `Track` / `Bus` (channel) | gain -> `volumeDb`, pan -> `pan` |
| `Track` (audio/MIDI) | `Track` (`kind: audio｜midi｜instrument`) | |
| `Playlist` / `Region` | `Clip` (`audio` waveform ｜ `midi` notes) | region bounds -> `startBar` / `lengthBars` |
| `Processor` / `Plugin` insert | `Insert` (mixer) / `Device` (chain) | `params` -> `KnobParam`; enable -> `enabled` |
| `Send` / internal aux | `Send` | level -> `amount` |
| `Amp` / `Panner` controls | `volumeDb` / `pan` | |
| Meters / level analysis | `MeterFrame` (`meter.update` event) | per-channel L/R; master LUFS / true-peak / GR |
| Master out | `MasterChannel` | loudness from engine analysis |

## Protocol

- **Commands (UI -> engine):** `EngineCommand` — transport, track, mixer,
  insert/send, device, clip, and the AI command layer (`ai.propose｜preview｜apply｜undo`).
- **Events (engine -> UI):** `EngineEvent` — `engine.ready`, `transport.position`,
  `meter.update`, `project.changed`, `ai.proposal`, `ai.applied`, `engine.error`.

All shapes are plain JSON, so the same contract serializes over any bridge.

## Bridge options (decision pending)

1. **Local sidecar over WebSocket** — a C++/`libardour` process exposes this
   contract as JSON over `ws://localhost`. UI stays a pure web app; engine is a
   separate, independently-testable process. Fastest to stand up and the easiest
   path to the headless smoke harness (E1-T3). **Recommended first integration.**
2. **Tauri** (Rust shell + system webview) — Rust bridges to `libardour` via C
   FFI; commands arrive through `invoke`, events via Tauri's event channel.
   Ships as one native binary; natural fit for the E2-T1 desktop shell. Good
   second step once the sidecar contract is proven.
3. **Electron** — Node shell with a native addon or child engine process.
   Heaviest; only if the team already standardizes on Electron.

**Suggested path:** stand up the sidecar (option 1) to prove the contract and
satisfy E1-T3, then wrap in Tauri (option 2) for a shippable desktop build.

## Status

- [x] E1-T1 — command/event schema (`contract.ts`)
- [x] E1-T2 — Ardour -> Zephyr concept mapping (this doc) + `EngineClient` seam + mock
- [ ] E1-T3 — headless engine smoke harness (needs the bridge decision above)
- [ ] Wire UI panels to `engine` instead of importing the fixture directly
