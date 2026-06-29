import { useEffect, useMemo, useRef, useState } from "react";
import { Minus, Plus, Play, Square, LayoutGrid } from "lucide-react";
import { cn } from "@/lib/cn";
import type { Clip, NoteRole } from "@/lib/types";
import { pianoRollDefaults, PIANO_ACCENT, type EditorNote } from "@/lib/editor-fixtures";
import { Segmented, type SegmentedOption } from "@/components/primitives";

/* ---- geometry (px) — matches PianoRoll.dc.html ---- */
const HI = 76;
const LO = 36;
const ROW = 14;
const BEAT = 84;
const SIX = 21; // one sixteenth
const BARS = 4;
const W = BARS * 4 * BEAT;
const ROWS = HI - LO + 1;
const GRID_H = ROWS * ROW;

const BLACK_PC = new Set([1, 3, 6, 8, 10]);
const SCALE_PC = new Set([5, 7, 8, 10, 0, 1, 3]); // F natural minor
const ROOT_PC = 5;
const NOTE_NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"];

type Tool = "draw" | "sel" | "cut";
const TOOLS: SegmentedOption<Tool>[] = [
  { key: "draw", label: "✎" },
  { key: "sel", label: "⬚" },
  { key: "cut", label: "✂" },
];

/** Internal note in pixel coordinates (mirrors the design's working model). */
interface PxNote {
  x: number;
  w: number;
  pitch: number;
  vel: number;
  role: NoteRole;
}

function lighten(hex: string, amt: number): string {
  const n = Number.parseInt(hex.slice(1), 16);
  let r = (n >> 16) & 255;
  let g = (n >> 8) & 255;
  let b = n & 255;
  r = Math.round(r + (255 - r) * amt);
  g = Math.round(g + (255 - g) * amt);
  b = Math.round(b + (255 - b) * amt);
  return `rgb(${r}, ${g}, ${b})`;
}

function rgba(hex: string, a: number): string {
  const n = Number.parseInt(hex.slice(1), 16);
  return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${a})`;
}

function noteName(m: number): string {
  return NOTE_NAMES[((m % 12) + 12) % 12] + (Math.floor(m / 12) - 1);
}

const ROLE_BRIGHT: Record<NoteRole, number> = { bass: -0.04, chord: -0.02, mel: 0.22 };

/** Seed editor notes from the clicked clip, or fall back to the demo notes. */
function seedNotes(clip?: Clip): PxNote[] {
  const source: EditorNote[] =
    clip && clip.type === "midi" && clip.notes.length
      ? clip.notes.map((n) => ({
          beat: n.bar * 4 + n.beat,
          length: n.length,
          pitch: n.pitch,
          velocity: n.velocity ?? 0.8,
          role: n.role ?? "mel",
        }))
      : pianoRollDefaults;
  return source.map((n) => ({
    x: n.beat * BEAT,
    w: n.length * BEAT,
    pitch: n.pitch,
    vel: n.velocity,
    role: n.role,
  }));
}

interface PianoRollProps {
  clip?: Clip;
  /** title subtitle, e.g. "Piano · Chorus" */
  subtitle?: string;
  onChannelRack: () => void;
}

/** MIDI editor: scale-aware note grid + keyboard + velocity lane. Port of
   PianoRoll.dc.html — draw notes by clicking the grid, double-click to delete. */
export function PianoRoll({ clip, subtitle = "Piano · Chorus", onChannelRack }: PianoRollProps) {
  const accent = PIANO_ACCENT;
  const [notes, setNotes] = useState<PxNote[]>(() => seedNotes(clip));
  const [sel, setSel] = useState(notes.length > 1 ? notes.length - 1 : -1);
  const [tool, setTool] = useState<Tool>("draw");
  const [playing, setPlaying] = useState(false);
  const [playX, setPlayX] = useState(8 * BEAT);
  const lastClip = useRef(clip);

  // reseed when a different clip opens the editor
  if (clip !== lastClip.current) {
    lastClip.current = clip;
    const next = seedNotes(clip);
    setNotes(next);
    setSel(next.length > 1 ? next.length - 1 : -1);
  }

  useEffect(() => {
    if (!playing) return;
    const id = window.setInterval(() => {
      setPlayX((x) => (x + SIX > W ? 0 : x + SIX));
    }, 107);
    return () => window.clearInterval(id);
  }, [playing]);

  const keys = useMemo(() => {
    const out: { label: string; black: boolean; isC: boolean }[] = [];
    for (let p = HI; p >= LO; p--) {
      const pc = ((p % 12) + 12) % 12;
      const isC = pc === 0;
      out.push({ label: isC ? noteName(p) : "", black: BLACK_PC.has(pc), isC });
    }
    return out;
  }, []);

  const rowBgs = useMemo(() => {
    const out: { bg: string; isC: boolean; isRoot: boolean }[] = [];
    for (let i = 0; i < ROWS; i++) {
      const p = HI - i;
      const pc = ((p % 12) + 12) % 12;
      const black = BLACK_PC.has(pc);
      const inScale = SCALE_PC.has(pc);
      let bg = black ? "#121214" : "#171719";
      if (inScale && !black) bg = rgba(accent, 0.05);
      out.push({ bg, isC: pc === 0, isRoot: pc === ROOT_PC });
    }
    return out;
  }, [accent]);

  const vlines = useMemo(() => {
    const out: { left: number; strong: boolean }[] = [];
    const beats = BARS * 4;
    for (let bt = 0; bt <= beats; bt++) {
      out.push({ left: bt * BEAT, strong: bt % 4 === 0 });
      if (bt < beats) for (let k = 1; k < 4; k++) out.push({ left: bt * BEAT + k * SIX, strong: false });
    }
    return out;
  }, []);

  const drawNote = (e: React.MouseEvent<HTMLDivElement>) => {
    if (e.target !== e.currentTarget) return;
    const rect = e.currentTarget.getBoundingClientRect();
    const ox = e.clientX - rect.left;
    const oy = e.clientY - rect.top;
    const x = Math.floor(ox / SIX) * SIX;
    const pitch = HI - Math.floor(oy / ROW);
    setNotes((prev) => {
      setSel(prev.length);
      return [...prev, { x, w: BEAT, pitch, vel: 0.8, role: "mel" }];
    });
  };

  const deleteNote = (i: number, e: React.MouseEvent) => {
    e.stopPropagation();
    setNotes((prev) => prev.filter((_, j) => j !== i));
    setSel(-1);
  };

  return (
    <div className="flex h-full w-full flex-col overflow-hidden bg-panel text-foreground">
      {/* ===== TOOLBAR ===== */}
      <div className="flex h-12 flex-none items-center gap-3 border-b border-white/[0.07] bg-rail px-3.5">
        <div className="flex items-center gap-2">
          <span
            className="h-[11px] w-[11px] rounded-[3px]"
            style={{ background: accent, boxShadow: `0 0 8px ${rgba(accent, 0.5)}` }}
          />
          <div className="flex flex-col leading-tight">
            <span className="text-[13px] font-semibold tracking-[-0.2px] text-ink">Piano Roll</span>
            <span className="text-[9.5px] text-ink-dim">{subtitle}</span>
          </div>
        </div>
        <span className="h-[18px] w-px bg-white/10" />
        <Segmented options={TOOLS} value={tool} onChange={setTool} size="sm" />
        <div className="flex items-center gap-1.5 text-[11px] text-ink-muted">
          <span className="rounded-md border border-white/10 px-2.5 py-1">Snap 1/16</span>
          <span className="rounded-md border border-white/10 px-2.5 py-1">Length 1/4</span>
          <span className="rounded-md bg-accent/10 px-2.5 py-1 font-medium text-accent">F min</span>
        </div>
        <div className="flex-1" />
        <button
          type="button"
          onClick={() => setPlaying((p) => !p)}
          className={cn(
            "flex h-8 w-8 items-center justify-center rounded-[9px] text-white",
            playing
              ? "bg-gradient-to-b from-[#ff6b60] to-[#e8453a] shadow-[0_2px_8px_rgba(232,69,58,0.5)]"
              : "bg-gradient-to-b from-[#2b97ff] to-[#0a78f0] shadow-[0_2px_8px_rgba(10,132,255,0.5)]",
          )}
        >
          {playing ? <Square size={13} fill="currentColor" /> : <Play size={13} fill="currentColor" />}
        </button>
        <div className="flex items-center gap-1.5 text-ink-muted">
          <Minus size={14} className="cursor-pointer" />
          <span className="relative h-[5px] w-[54px] rounded-full bg-white/10">
            <span className="absolute left-[58%] top-1/2 h-3 w-3 -translate-x-1/2 -translate-y-1/2 rounded-full bg-white shadow-[0_1px_3px_rgba(0,0,0,0.5)]" />
          </span>
          <Plus size={14} className="cursor-pointer" />
        </div>
        <span className="h-[18px] w-px bg-white/10" />
        <button
          type="button"
          onClick={onChannelRack}
          className="flex items-center gap-1.5 rounded-lg border border-accent/30 bg-accent/10 px-2.5 py-1.5 text-[11.5px] text-accent"
        >
          <LayoutGrid size={13} />
          Channel Rack
        </button>
      </div>

      {/* ===== RULER ===== */}
      <div className="flex h-[26px] flex-none border-b border-white/10 bg-rail">
        <div className="w-[62px] flex-none border-r border-white/[0.07]" />
        <div className="relative flex-1 overflow-hidden">
          <div className="absolute inset-y-0 left-0" style={{ width: W }}>
            {Array.from({ length: BARS }, (_, b) => (
              <div
                key={b}
                className="absolute top-0 flex h-full items-center border-l border-white/10 pl-2.5 font-mono text-[10px] font-semibold text-ink-secondary"
                style={{ left: b * 4 * BEAT, width: 4 * BEAT }}
              >
                {b + 1}
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* ===== GRID ROW ===== */}
      <div className="flex flex-none" style={{ height: GRID_H }}>
        {/* keyboard */}
        <div className="w-[62px] flex-none overflow-hidden border-r border-white/[0.07] bg-sunken">
          {keys.map((k, i) => (
            <div
              key={i}
              className="flex items-center justify-end pr-1.5"
              style={{
                height: ROW,
                background: k.black ? "#0a0a0c" : "#1d1d20",
                borderBottom: k.isC ? "1px solid rgba(255,255,255,0.14)" : "1px solid rgba(0,0,0,0.4)",
              }}
            >
              <span
                className="text-[8.5px] font-semibold"
                style={{ color: k.black ? "#6b6b70" : "#9a9aa0" }}
              >
                {k.label}
              </span>
            </div>
          ))}
        </div>
        {/* note grid */}
        <div className="relative flex-1 overflow-x-auto overflow-y-hidden">
          <div
            className="relative cursor-crosshair"
            style={{ width: W, height: GRID_H }}
            onClick={drawNote}
          >
            {/* row backgrounds */}
            {rowBgs.map((r, i) => (
              <div
                key={`row-${i}`}
                className="pointer-events-none absolute inset-x-0"
                style={{
                  top: i * ROW,
                  height: ROW,
                  background: r.bg,
                  borderBottom: `1px solid rgba(255,255,255,${r.isC ? 0.09 : 0.03})`,
                  boxShadow: r.isRoot ? `inset 0 0 0 0.5px ${rgba(accent, 0.18)}` : undefined,
                }}
              />
            ))}
            {/* vertical gridlines */}
            {vlines.map((v, i) => (
              <div
                key={`v-${i}`}
                className="pointer-events-none absolute inset-y-0"
                style={{ left: v.left, width: 1, background: `rgba(255,255,255,${v.strong ? 0.1 : 0.022})` }}
              />
            ))}
            {/* notes */}
            {notes.map((n, i) => {
              const seld = i === sel;
              const top = (HI - n.pitch) * ROW;
              const baseCol = seld ? "#2997ff" : lighten(accent, ROLE_BRIGHT[n.role]);
              const lite = lighten(seld ? "#2997ff" : accent, ROLE_BRIGHT[n.role] + 0.28);
              return (
                <div
                  key={`n-${i}`}
                  onClick={(e) => {
                    e.stopPropagation();
                    setSel(i);
                  }}
                  onDoubleClick={(e) => deleteNote(i, e)}
                  className="absolute cursor-pointer overflow-hidden rounded-[3px]"
                  style={{
                    left: n.x,
                    top: top + 1,
                    width: n.w - 1,
                    height: ROW - 2,
                    background: `linear-gradient(180deg, ${lite}, ${baseCol})`,
                    boxShadow: `inset 0 1px 0 rgba(255,255,255,0.4), 0 1px 3px rgba(0,0,0,0.4)${seld ? ", 0 0 0 1.5px rgba(41,151,255,0.5)" : ""}`,
                    borderLeft: `2px solid ${seld ? "#7cc0ff" : lighten(accent, 0.45)}`,
                    opacity: 0.55 + n.vel * 0.45,
                    zIndex: seld ? 5 : 2,
                  }}
                >
                  <div className="absolute bottom-0 left-0 h-[2px] w-full bg-black/25" />
                </div>
              );
            })}
            {/* playhead */}
            <div
              className="pointer-events-none absolute inset-y-0 z-[8] w-[1.5px] bg-[#f4f4f6] shadow-[0_0_8px_rgba(255,255,255,0.3)]"
              style={{ left: playX }}
            >
              <div className="absolute -left-1 -top-px h-1.5 w-[9px] rounded-[1px] bg-[#f4f4f6]" />
            </div>
          </div>
        </div>
      </div>

      {/* ===== VELOCITY LANE ===== */}
      <div className="flex min-h-0 flex-1 border-t border-white/10 bg-rail">
        <div className="flex w-[62px] flex-none flex-col justify-center border-r border-white/[0.07] px-2.5">
          <span className="text-[9px] font-semibold uppercase tracking-wide text-ink-dim">Velocity</span>
          <span className="mt-0.5 text-[9px] text-ink-faint">0–127</span>
        </div>
        <div className="relative flex-1 overflow-hidden">
          <div className="absolute inset-y-0 left-0" style={{ width: W }}>
            {notes.map((n, i) => {
              const seld = i === sel;
              const col = seld ? "#2997ff" : accent;
              return (
                <div
                  key={`vel-${i}`}
                  className="absolute bottom-0 w-[3px] rounded-t-sm"
                  style={{ left: n.x + 1, height: `${18 + n.vel * 70}%`, background: rgba(col, seld ? 0.95 : 0.7) }}
                >
                  <div
                    className="absolute -top-[3px] left-1/2 h-[7px] w-[7px] -translate-x-1/2 rounded-full"
                    style={{ background: col, boxShadow: `0 0 6px ${rgba(col, 0.6)}` }}
                  />
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </div>
  );
}
