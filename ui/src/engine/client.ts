/* The engine seam (EPIC 1).

   `EngineClient` is the single interface the UI talks to. Today the UI runs on
   `MockEngineClient` (backed by the demo fixture); swapping in a real
   transport-backed client (Tauri/WebSocket/IPC) later requires no UI changes. */

import type { Bus, Project, Track } from "@/lib/types";
import { project as demoProject } from "@/lib/project";
import type { CommandAck, EngineCommand, EngineEvent } from "./contract";

export interface EngineClient {
  /** Connect and return the initial session snapshot. */
  connect(): Promise<Project>;
  /** Send a command; resolves once the engine acknowledges it. */
  send(cmd: EngineCommand): Promise<CommandAck>;
  /** Subscribe to engine events; returns an unsubscribe function. */
  subscribe(handler: (event: EngineEvent) => void): () => void;
  disconnect(): void;
}

type Handler = (event: EngineEvent) => void;

const OK: CommandAck = { ok: true };

/** In-memory engine stand-in. Deterministic, no timers — applies a useful
   subset of commands to a working copy and emits project.changed. */
export class MockEngineClient implements EngineClient {
  private project: Project;
  private handlers = new Set<Handler>();

  constructor(initial: Project = demoProject) {
    this.project = structuredClone(initial);
  }

  async connect(): Promise<Project> {
    this.emit({ type: "engine.ready", project: this.project });
    return this.project;
  }

  async send(cmd: EngineCommand): Promise<CommandAck> {
    switch (cmd.type) {
      case "mixer.setVolume":
        this.patchChannel(cmd.channelId, (c) => (c.volumeDb = cmd.db));
        break;
      case "mixer.setPan":
        this.patchChannel(cmd.channelId, (c) => (c.pan = cmd.pan));
        break;
      case "mixer.setMute":
        this.patchTrack(cmd.channelId, (t) => (t.muted = cmd.muted));
        break;
      case "mixer.setSolo":
        this.patchTrack(cmd.channelId, (t) => (t.soloed = cmd.soloed));
        break;
      case "mixer.setArm":
        this.patchTrack(cmd.trackId, (t) => (t.recordArmed = cmd.armed));
        break;
      case "device.setParam":
        this.patchTrack(cmd.trackId, (t) => {
          const d = t.devices.find((x) => x.id === cmd.deviceId);
          const p = d?.params.find((x) => x.id === cmd.paramId);
          if (p) p.value = cmd.value;
        });
        break;
      case "device.toggle":
        this.patchTrack(cmd.trackId, (t) => {
          const d = t.devices.find((x) => x.id === cmd.deviceId);
          if (d) d.enabled = cmd.enabled;
        });
        break;
      case "ai.propose":
        this.emit({
          type: "ai.proposal",
          proposal: { id: "proposal-1", summary: cmd.prompt, diff: this.project.diff },
        });
        return OK;
      case "ai.apply":
        this.emit({ type: "ai.applied", proposalId: cmd.proposalId });
        return OK;
      default:
        // transport / clip / track / insert commands: accepted, not modelled here
        return OK;
    }
    this.emit({ type: "project.changed", project: this.project });
    return OK;
  }

  subscribe(handler: Handler): () => void {
    this.handlers.add(handler);
    return () => this.handlers.delete(handler);
  }

  disconnect(): void {
    this.handlers.clear();
  }

  private emit(event: EngineEvent) {
    for (const h of this.handlers) h(event);
  }

  private channel(id: string): Track | Bus | undefined {
    return (
      this.project.tracks.find((t) => t.id === id) ??
      this.project.buses.find((b) => b.id === id)
    );
  }

  private patchChannel(id: string, fn: (c: Track | Bus) => void) {
    const c = this.channel(id);
    if (c) fn(c);
  }

  private patchTrack(id: string, fn: (t: Track) => void) {
    const t = this.project.tracks.find((x) => x.id === id);
    if (t) fn(t);
  }
}

/** Default client the app boots with. Replace with a real transport-backed
   implementation once the engine bridge is chosen. */
export const engine: EngineClient = new MockEngineClient();
