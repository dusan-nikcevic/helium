/* Engine boundary contract (EPIC 1 / E1-T1).

   A transport-agnostic command + event protocol between the Zephyr UI and the
   Ardour-backed engine. The same shapes serialize over any bridge (Tauri
   invoke, a local WebSocket sidecar, Electron IPC, ...). The UI depends only on
   this contract and the EngineClient interface, never on a concrete transport. */

import type { Diff, Project, TrackColor, TrackKind } from "@/lib/types";

/* ---- Commands: UI -> engine -------------------------------------------- */

export type TransportCommand =
  | { type: "transport.play" }
  | { type: "transport.stop" }
  | { type: "transport.record" }
  | { type: "transport.toggleLoop" }
  | { type: "transport.toggleMetronome" }
  | { type: "transport.locate"; bar: number; beat: number; tick: number };

export type TrackCommand =
  | { type: "track.create"; kind: TrackKind; name?: string }
  | { type: "track.rename"; trackId: string; name: string }
  | { type: "track.setColor"; trackId: string; color: TrackColor }
  | { type: "track.route"; trackId: string; busId: string };

export type MixerCommand =
  | { type: "mixer.setVolume"; channelId: string; db: number }
  | { type: "mixer.setPan"; channelId: string; pan: number }
  | { type: "mixer.setMute"; channelId: string; muted: boolean }
  | { type: "mixer.setSolo"; channelId: string; soloed: boolean }
  | { type: "mixer.setArm"; trackId: string; armed: boolean };

export type InsertCommand =
  | { type: "insert.add"; channelId: string; name: string; position?: number }
  | { type: "insert.toggle"; channelId: string; insertId: string; enabled: boolean }
  | { type: "insert.remove"; channelId: string; insertId: string }
  | { type: "send.set"; channelId: string; busId: string; amount: number };

export type DeviceCommand =
  | { type: "device.toggle"; trackId: string; deviceId: string; enabled: boolean }
  | { type: "device.setParam"; trackId: string; deviceId: string; paramId: string; value: number };

export type ClipCommand =
  | { type: "clip.create"; trackId: string; startBar: number; lengthBars: number }
  | { type: "clip.move"; clipId: string; startBar: number; trackId?: string }
  | { type: "clip.resize"; clipId: string; lengthBars: number }
  | { type: "clip.select"; clipId: string | null };

/** The Cursor-like AI command layer: propose -> preview -> apply/undo. */
export type AiCommand =
  | { type: "ai.propose"; prompt: string; targetIds?: string[] }
  | { type: "ai.preview"; proposalId: string }
  | { type: "ai.apply"; proposalId: string }
  | { type: "ai.undo"; proposalId?: string };

export type EngineCommand =
  | TransportCommand
  | TrackCommand
  | MixerCommand
  | InsertCommand
  | DeviceCommand
  | ClipCommand
  | AiCommand;

export type EngineCommandType = EngineCommand["type"];

/* ---- Events: engine -> UI ---------------------------------------------- */

export interface TransportState {
  bar: number;
  beat: number;
  tick: number;
  seconds: number;
  playing: boolean;
  recording: boolean;
  loop: boolean;
}

export interface ChannelLevels {
  l: number;
  r: number;
}

export interface MeterFrame {
  /** per-channel (track + bus) instantaneous levels, keyed by channel id */
  channels: Record<string, ChannelLevels>;
  master: ChannelLevels & { lufs: number; truePeak: number; gr: number };
  cpu: number;
}

/** A pending AI proposal the user can preview/apply/undo. */
export interface AiProposal {
  id: string;
  summary: string;
  diff: Diff;
}

export type EngineEvent =
  | { type: "engine.ready"; project: Project }
  | { type: "transport.position"; state: TransportState }
  | { type: "meter.update"; frame: MeterFrame }
  | { type: "project.changed"; project: Project }
  | { type: "ai.proposal"; proposal: AiProposal }
  | { type: "ai.applied"; proposalId: string }
  | { type: "engine.error"; message: string };

export type EngineEventType = EngineEvent["type"];

/** Result of a command the engine acknowledges synchronously. */
export interface CommandAck {
  ok: boolean;
  /** optional error when ok is false */
  error?: string;
}
