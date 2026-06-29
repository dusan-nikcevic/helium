/* Domain model for the Aria DAW UI. Pure view types — no engine coupling yet.
   Track/bus/device shapes mirror the Ardour-backed Zephyr engine concepts the
   design represents (routes, inserts, sends, plugins). */

export type TrackColor =
  | "drums"
  | "bass"
  | "vocal"
  | "vocaldbl"
  | "piano"
  | "pads"
  | "guitars"
  | "fx"
  | "master";

/** Tailwind text/bg class fragments keyed by TrackColor (token-backed). */
export const TRACK_HEX: Record<TrackColor, string> = {
  drums: "#cf8163",
  bass: "#7e84c0",
  vocal: "#5aa9a0",
  vocaldbl: "#5d93b3",
  piano: "#bd9b62",
  pads: "#9b7cb8",
  guitars: "#8c9b66",
  fx: "#b66f80",
  master: "#9aa0a6",
};

export type DeviceKind =
  | "eq"
  | "dynamics"
  | "saturation"
  | "fx"
  | "instrument"
  | "utility";

export const DEVICE_HEX: Record<DeviceKind, string> = {
  eq: "#6f9fc8",
  dynamics: "#e0a955",
  saturation: "#d9774f",
  fx: "#9a7cb8",
  instrument: "#5aa9a0",
  utility: "#8a8f96",
};

export interface Insert {
  id: string;
  name: string;
  enabled: boolean;
  aiEdited?: boolean;
}

export interface Send {
  id: string;
  /** destination bus display name */
  name: string;
  /** 0..100 percent */
  amount: number;
  aiEdited?: boolean;
}

export interface KnobParam {
  id: string;
  label: string;
  /** normalized 0..1 */
  value: number;
  /** formatted display value, e.g. "250 Hz" */
  display?: string;
  aiEdited?: boolean;
}

export type GraphKind = "eq" | "comp" | null;

export interface Device {
  id: string;
  name: string;
  kind: DeviceKind;
  preset?: string;
  enabled: boolean;
  aiEdited?: boolean;
  params: KnobParam[];
  /** response-curve graphic to draw above the knob grid */
  graph?: GraphKind;
}

/** Musical role of a midi note — drives note coloring in the piano roll. */
export type NoteRole = "bass" | "chord" | "mel";

export interface MidiNote {
  /** absolute bar offset within the clip */
  bar: number;
  beat: number;
  /** MIDI pitch number */
  pitch: number;
  /** length in beats */
  length: number;
  /** normalized 0..1 velocity (piano roll); optional on arrangement fixtures */
  velocity?: number;
  /** musical role used for note coloring in the piano roll */
  role?: NoteRole;
}

interface ClipBase {
  id: string;
  name: string;
  startBar: number;
  lengthBars: number;
  color: TrackColor;
  selected?: boolean;
}

export interface AudioClip extends ClipBase {
  type: "audio";
  /** deterministic seed so the waveform renders stably */
  seed: number;
}

export interface MidiClip extends ClipBase {
  type: "midi";
  notes: MidiNote[];
}

export type Clip = AudioClip | MidiClip;

export type TrackKind = "audio" | "midi" | "instrument";

export interface Track {
  id: string;
  name: string;
  /** short badge text, e.g. "Dr" */
  short: string;
  color: TrackColor;
  kind: TrackKind;
  muted?: boolean;
  soloed?: boolean;
  recordArmed?: boolean;
  /** output bus id */
  busId?: string;
  volumeDb: number;
  /** -1..1 */
  pan: number;
  inserts: Insert[];
  sends: Send[];
  devices: Device[];
  clips: Clip[];
  /** 0..1 current meter levels */
  meterL: number;
  meterR: number;
  aiEdited?: boolean;
}

export interface Bus {
  id: string;
  name: string;
  color: TrackColor;
  /** routing summary, e.g. "Drum Bus -> Master" */
  routeLabel: string;
  output: string;
  memberCount: number;
  volumeDb: number;
  pan: number;
  inserts: Insert[];
  sends: Send[];
  meterL: number;
  meterR: number;
  aiEdited?: boolean;
}

export interface Group {
  id: string;
  name: string;
  color: TrackColor;
  busId: string;
  /** track ids in this folder group, followed by the bus strip */
  trackIds: string[];
  collapsed?: boolean;
}

export interface MasterChannel {
  name: string;
  /** integrated LUFS, e.g. -9.4 */
  lufsIntegrated: number;
  /** true peak dBTP, e.g. -1.0 */
  truePeak: number;
  /** gain reduction dB, e.g. -2.1 */
  gainReduction: number;
  inserts: Insert[];
  volumeDb: number;
  meterL: number;
  meterR: number;
}

export interface ChatMessage {
  id: string;
  role: "user" | "assistant";
  text: string;
}

export interface DiffChange {
  id: string;
  trackName: string;
  color: TrackColor;
  /** what changed, e.g. "Channel EQ - 250 Hz dip" */
  description: string;
  /** formatted delta, e.g. "-2.8 dB" or "+8%" */
  delta: string;
  direction: "down" | "up" | "neutral";
}

export interface Diff {
  /** e.g. "4 changes" */
  title: string;
  /** section badge, e.g. "Chorus" */
  section: string;
  changes: DiffChange[];
}

export interface AiHistoryItem {
  id: string;
  text: string;
  time: string;
}

export interface BrowserItem {
  id: string;
  name: string;
  tag: string;
  color: TrackColor;
}

export interface SectionMarker {
  id: string;
  name: string;
  startBar: number;
  lengthBars: number;
  highlight?: boolean;
}

export type WorkspaceView = "arrange" | "mix" | "split" | "session";
export type AiMode = "inspector" | "spotlight";
/** Which editor the clip-detail modal shows. */
export type ClipEditorMode = "piano" | "pattern";

export interface Project {
  name: string;
  section: string;
  key: string;
  bpm: number;
  timeSig: string;
  sampleRate: string;
  engine: string;
  /** total bars shown on the arrangement ruler */
  totalBars: number;
  /** first visible bar number on the ruler */
  startBar: number;
  sections: SectionMarker[];
  tracks: Track[];
  buses: Bus[];
  groups: Group[];
  master: MasterChannel;
  chat: ChatMessage[];
  diff: Diff;
  aiHistory: AiHistoryItem[];
  browserInstruments: BrowserItem[];
}
