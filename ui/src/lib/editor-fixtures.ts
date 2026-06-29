/* Seed data for the clip-detail editors (Piano Roll + Pattern Designer).
   Ported from the Claude Design references PianoRoll.dc.html (buildNotes) and
   PatternDesigner.dc.html (buildChannels). Values are fixed (no RNG) so the
   editors render deterministically. */

import type { NoteRole } from "./types";

/** A note in piano-roll coordinates: absolute beats from the clip start. */
export interface EditorNote {
  /** start position in beats (0-based) */
  beat: number;
  /** length in beats */
  length: number;
  /** MIDI pitch number */
  pitch: number;
  /** normalized 0..1 velocity */
  velocity: number;
  role: NoteRole;
}

/** F-minor bass + chord + melody, 4 bars (16 beats) — the design's demo clip. */
export const pianoRollDefaults: EditorNote[] = [
  // bass (i VI III VII in F minor)
  { beat: 0, length: 4, pitch: 41, velocity: 0.92, role: "bass" },
  { beat: 4, length: 4, pitch: 37, velocity: 0.92, role: "bass" },
  { beat: 8, length: 4, pitch: 44, velocity: 0.92, role: "bass" },
  { beat: 12, length: 4, pitch: 39, velocity: 0.92, role: "bass" },
  // chords
  ...[
    [0, [53, 56, 60]],
    [4, [49, 53, 56]],
    [8, [56, 60, 63]],
    [12, [51, 55, 58]],
  ].flatMap(([t, chord]) =>
    (chord as number[]).map(
      (pitch): EditorNote => ({ beat: t as number, length: 4, pitch, velocity: 0.66, role: "chord" }),
    ),
  ),
  // melody
  { beat: 0, length: 1, pitch: 72, velocity: 0.86, role: "mel" },
  { beat: 1, length: 1, pitch: 73, velocity: 0.8, role: "mel" },
  { beat: 2, length: 2, pitch: 72, velocity: 0.88, role: "mel" },
  { beat: 4, length: 1, pitch: 68, velocity: 0.84, role: "mel" },
  { beat: 5, length: 1, pitch: 65, velocity: 0.78, role: "mel" },
  { beat: 6, length: 2, pitch: 63, velocity: 0.86, role: "mel" },
  { beat: 8, length: 1, pitch: 63, velocity: 0.8, role: "mel" },
  { beat: 9, length: 1, pitch: 72, velocity: 0.9, role: "mel" },
  { beat: 10, length: 2, pitch: 70, velocity: 0.86, role: "mel" },
  { beat: 12, length: 1, pitch: 70, velocity: 0.84, role: "mel" },
  { beat: 13, length: 1, pitch: 68, velocity: 0.8, role: "mel" },
  { beat: 14, length: 2, pitch: 75, velocity: 0.92, role: "mel" },
];

/** Accent color for the piano-roll surface (Piano track gold). */
export const PIANO_ACCENT = "#bd9b62";

export interface PatternChannel {
  name: string;
  sub: string;
  /** hex color */
  color: string;
  /** active step indices (0..15) */
  on: number[];
  /** normalized 0..1 */
  vol: number;
  pan: number;
  cut: number;
  drive: number;
}

/** FL-style channel rack — 9 channels of a 16-step pattern. */
export const patternChannels: PatternChannel[] = [
  { name: "Kick", sub: "808 Kick 03", color: "#cf8163", on: [0, 4, 8, 11, 12], vol: 0.78, pan: 0.5, cut: 0.42, drive: 0.55 },
  { name: "Clap", sub: "Vintage Clap", color: "#b66f80", on: [4, 12], vol: 0.74, pan: 0.5, cut: 0.6, drive: 0.3 },
  { name: "Snare", sub: "Layered Snr", color: "#bd9b62", on: [4, 12], vol: 0.8, pan: 0.5, cut: 0.5, drive: 0.45 },
  { name: "Hat Cl", sub: "Closed Hat", color: "#5aa9a0", on: [0, 2, 4, 6, 8, 10, 12, 14], vol: 0.7, pan: 0.5, cut: 0.78, drive: 0.2 },
  { name: "Hat Op", sub: "Open Hat", color: "#5d93b3", on: [2, 6, 10, 14], vol: 0.72, pan: 0.5, cut: 0.66, drive: 0.25 },
  { name: "808", sub: "Sub Bass", color: "#7e84c0", on: [0, 8, 11, 14], vol: 0.82, pan: 0.5, cut: 0.34, drive: 0.6 },
  { name: "Rim", sub: "Side Stick", color: "#8c9b66", on: [6, 10], vol: 0.68, pan: 0.5, cut: 0.55, drive: 0.3 },
  { name: "Perc", sub: "Shaker Loop", color: "#9b7cb8", on: [3, 7, 11, 15], vol: 0.7, pan: 0.5, cut: 0.7, drive: 0.22 },
  { name: "FX", sub: "Riser Hit", color: "#7e9bb8", on: [0], vol: 0.76, pan: 0.5, cut: 0.5, drive: 0.4 },
];

export const patternBanks = ["A1", "A2", "B1", "Fill"] as const;
