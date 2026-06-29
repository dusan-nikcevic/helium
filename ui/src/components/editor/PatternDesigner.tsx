import { useEffect, useMemo, useState } from "react";
import { Play, Square, Piano } from "lucide-react";
import { cn } from "@/lib/cn";
import { Knob, Waveform } from "@/components/primitives";
import { Segmented, type SegmentedOption } from "@/components/primitives";
import { patternBanks, patternChannels } from "@/lib/editor-fixtures";

interface Step {
  on: boolean;
  vel: number;
}
interface Channel {
  name: string;
  sub: string;
  color: string;
  muted: boolean;
  steps: Step[];
  vol: number;
  pan: number;
  cut: number;
  drive: number;
}

function rgba(hex: string, a: number): string {
  const n = Number.parseInt(hex.slice(1), 16);
  return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${a})`;
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

function buildChannels(): Channel[] {
  return patternChannels.map((d) => ({
    name: d.name,
    sub: d.sub,
    color: d.color,
    muted: false,
    steps: Array.from({ length: 16 }, (_, s) => ({ on: d.on.includes(s), vel: 0.85 })),
    vol: d.vol,
    pan: d.pan,
    cut: d.cut,
    drive: d.drive,
  }));
}

const fmtDb = (v: number) => `${(v - 0.78) * 30 >= 0 ? "+" : ""}${((v - 0.78) * 30).toFixed(1)} dB`;
const fmtPan = (v: number) => {
  const p = Math.round((v - 0.5) * 200);
  return p === 0 ? "C" : p < 0 ? `${Math.abs(p)}L` : `${p}R`;
};

type Bank = (typeof patternBanks)[number];
const BANK_OPTS: SegmentedOption<Bank>[] = patternBanks.map((b) => ({ key: b, label: b }));

interface PatternDesignerProps {
  onPianoRoll: () => void;
}

/** FL-style channel rack: 16-step sequencer + per-channel inspector. Port of
   PatternDesigner.dc.html — click steps to program a beat. */
export function PatternDesigner({ onPianoRoll }: PatternDesignerProps) {
  const [channels, setChannels] = useState<Channel[]>(buildChannels);
  const [sel, setSel] = useState(0);
  const [bank, setBank] = useState<Bank>("A1");
  const [playing, setPlaying] = useState(false);
  const [playStep, setPlayStep] = useState(0);

  useEffect(() => {
    if (!playing) return;
    const id = window.setInterval(() => setPlayStep((s) => (s + 1) % 16), 107);
    return () => window.clearInterval(id);
  }, [playing]);

  const toggleStep = (ci: number, si: number) =>
    setChannels((prev) =>
      prev.map((c, i) =>
        i !== ci ? c : { ...c, steps: c.steps.map((st, j) => (j === si ? { ...st, on: !st.on } : st)) },
      ),
    );
  const toggleMute = (ci: number) =>
    setChannels((prev) => prev.map((c, i) => (i === ci ? { ...c, muted: !c.muted } : c)));

  const current = channels[sel];
  const activeCount = current.steps.filter((s) => s.on).length;

  const macros = useMemo(
    () => [
      { label: "Vol", v: current.vol, value: fmtDb(current.vol) },
      { label: "Pan", v: current.pan, value: fmtPan(current.pan) },
      { label: "Cutoff", v: current.cut, value: String(Math.round(200 + current.cut * 16000)) },
      { label: "Drive", v: current.drive, value: `${Math.round(current.drive * 100)}%` },
    ],
    [current],
  );

  return (
    <div className="flex h-full w-full flex-col overflow-hidden bg-panel text-foreground">
      {/* ===== SUB TOOLBAR ===== */}
      <div className="flex h-12 flex-none items-center gap-3 border-b border-white/[0.07] bg-rail px-3.5">
        <div className="flex items-center gap-2">
          <span className="flex h-[22px] w-[22px] items-center justify-center rounded-md bg-gradient-to-br from-[#3a3a40] to-[#1d1d20]">
            <Piano size={13} className="text-ink-secondary" />
          </span>
          <span className="text-[13px] font-semibold tracking-[-0.2px] text-ink">Channel Rack</span>
        </div>
        <span className="h-[18px] w-px bg-white/10" />
        <div className="flex items-center gap-1.5">
          <span className="text-[10px] font-semibold uppercase tracking-wide text-ink-dim">Pattern</span>
          <Segmented options={BANK_OPTS} value={bank} onChange={setBank} size="sm" />
        </div>
        <span className="h-[18px] w-px bg-white/10" />
        <div className="flex items-center gap-1.5">
          <button
            type="button"
            onClick={() => {
              setPlaying((p) => !p);
              setPlayStep(0);
            }}
            className={cn(
              "flex h-8 w-8 items-center justify-center rounded-[9px] text-white",
              playing
                ? "bg-gradient-to-b from-[#ff6b60] to-[#e8453a] shadow-[0_2px_8px_rgba(232,69,58,0.5)]"
                : "bg-gradient-to-b from-[#2b97ff] to-[#0a78f0] shadow-[0_2px_8px_rgba(10,132,255,0.5)]",
            )}
          >
            {playing ? <Square size={13} fill="currentColor" /> : <Play size={13} fill="currentColor" />}
          </button>
          <div className="flex flex-col leading-tight">
            <span className="font-mono text-[12px] font-bold text-ink-secondary">
              140<span className="ml-0.5 text-[8px] font-semibold text-ink-dim">BPM</span>
            </span>
            <span className="text-[8.5px] font-medium tracking-wide text-ink-muted">16 STEPS · 1/16</span>
          </div>
        </div>
        <div className="flex-1" />
        <div className="flex items-center gap-2 text-ink-muted">
          <span className="text-[11px]">Swing</span>
          <Knob value={0.24} size={22} color="#86868b" />
          <span className="font-mono text-[11px] text-ink-secondary">24%</span>
        </div>
        <span className="h-[18px] w-px bg-white/10" />
        <button
          type="button"
          onClick={onPianoRoll}
          className="flex items-center gap-1.5 rounded-lg border border-accent/30 bg-accent/10 px-2.5 py-1.5 text-[11.5px] text-accent"
        >
          <Piano size={13} />
          Open Piano Roll
        </button>
      </div>

      {/* ===== MAIN ===== */}
      <div className="flex min-h-0 flex-1">
        {/* ===== RACK ===== */}
        <div className="flex min-w-0 flex-1 flex-col bg-panel">
          {/* ruler */}
          <div className="flex h-[26px] flex-none items-stretch border-b border-white/10 bg-rail">
            <div className="flex w-[230px] flex-none items-center border-r border-white/[0.07] px-3.5 text-[10px] font-semibold uppercase tracking-wide text-ink-dim">
              Channel
            </div>
            <div className="flex flex-1 items-center gap-2.5 px-3.5">
              {Array.from({ length: 4 }, (_, g) => (
                <div key={g} className="flex flex-1 gap-1.5">
                  {Array.from({ length: 4 }, (_, k) => {
                    const idx = g * 4 + k;
                    const isBeat = k === 0;
                    const isPlay = playing && idx === playStep;
                    return (
                      <div
                        key={k}
                        className="flex-1 text-center font-mono text-[9px] font-semibold"
                        style={{ color: isPlay ? "#fff" : isBeat ? "#9a9aa0" : "#56565b" }}
                      >
                        {isBeat ? g + 1 : ""}
                      </div>
                    );
                  })}
                </div>
              ))}
            </div>
          </div>
          {/* channel rows */}
          <div className="flex-1 overflow-y-auto overflow-x-hidden">
            {channels.map((c, ci) => {
              const seld = ci === sel;
              return (
                <div
                  key={ci}
                  className="flex h-[46px] items-stretch border-b border-white/5"
                  style={{ background: seld ? "rgba(41,151,255,0.06)" : "transparent" }}
                >
                  {/* header */}
                  <div
                    className="relative flex w-[230px] flex-none items-center gap-2.5 border-r border-white/[0.07] pl-3.5 pr-3"
                    style={{ boxShadow: seld ? "inset 2px 0 0 #2997ff" : undefined }}
                  >
                    <span
                      className="absolute inset-y-0 left-0 w-[3px]"
                      style={{ background: c.color }}
                    />
                    <button
                      type="button"
                      onClick={() => toggleMute(ci)}
                      className="h-2.5 w-2.5 flex-none rounded-full"
                      style={{
                        background: c.muted ? "#3a3a3e" : lighten(c.color, 0.05),
                        boxShadow: c.muted ? "inset 0 0 0 1px rgba(255,255,255,0.12)" : `0 0 7px ${rgba(c.color, 0.7)}`,
                      }}
                    />
                    <button
                      type="button"
                      onClick={() => setSel(ci)}
                      className="flex min-w-0 flex-1 cursor-pointer flex-col text-left"
                    >
                      <span className="truncate text-[12px] font-medium tracking-[-0.2px] text-ink-secondary">{c.name}</span>
                      <span className="text-[9px] tracking-wide text-ink-dim">{c.sub}</span>
                    </button>
                    <Knob value={c.pan} size={24} color="#8a8a90" />
                    <Knob value={c.vol} size={24} color={c.color} />
                  </div>
                  {/* steps */}
                  <div className="flex flex-1 items-center gap-2.5 px-3.5">
                    {Array.from({ length: 4 }, (_, g) => (
                      <div key={g} className="flex flex-1 gap-1.5">
                        {Array.from({ length: 4 }, (_, k) => {
                          const si = g * 4 + k;
                          const stp = c.steps[si];
                          const beat = k === 0;
                          const isPlay = playing && si === playStep;
                          let style: React.CSSProperties;
                          if (stp.on && !c.muted) {
                            style = {
                              background: `linear-gradient(180deg, ${lighten(c.color, 0.32)}, ${c.color})`,
                              boxShadow: `inset 0 1px 0 rgba(255,255,255,0.45), 0 0 9px ${rgba(c.color, 0.5)}`,
                              filter: `brightness(${(0.66 + stp.vel * 0.4).toFixed(2)})`,
                            };
                          } else if (stp.on && c.muted) {
                            style = {
                              background: rgba(c.color, 0.22),
                              boxShadow: `inset 0 0 0 1px ${rgba(c.color, 0.35)}`,
                            };
                          } else {
                            style = {
                              background: beat ? "#151517" : "#0d0d0f",
                              boxShadow: `inset 0 0 0 1px rgba(255,255,255,${beat ? 0.07 : 0.04}), inset 0 1px 2px rgba(0,0,0,0.5)`,
                            };
                          }
                          if (isPlay) {
                            style.outline = "1.5px solid rgba(255,255,255,0.6)";
                            style.outlineOffset = "1px";
                            if (!stp.on) style.background = "rgba(255,255,255,0.07)";
                          }
                          return (
                            <button
                              key={k}
                              type="button"
                              onClick={() => toggleStep(ci, si)}
                              className="h-[26px] flex-1 rounded-[5px]"
                              style={style}
                            />
                          );
                        })}
                      </div>
                    ))}
                  </div>
                </div>
              );
            })}
            <div className="flex h-[42px] items-center gap-2 px-4.5 text-[11.5px] text-ink-faint">
              <span className="text-[15px]">+</span> Add channel
            </div>
          </div>
        </div>

        {/* ===== CHANNEL INSPECTOR ===== */}
        <div className="flex w-[316px] flex-none flex-col border-l border-white/[0.07] bg-panel">
          <div className="flex items-center gap-2.5 border-b border-white/[0.06] px-4 py-3">
            <span
              className="h-[13px] w-[13px] flex-none rounded"
              style={{ background: current.color, boxShadow: `0 0 8px ${rgba(current.color, 0.5)}` }}
            />
            <div className="min-w-0 flex-1">
              <div className="text-[13px] font-semibold tracking-[-0.2px] text-ink">{current.name}</div>
              <div className="text-[10px] text-ink-dim">{current.sub}</div>
            </div>
            <span className="rounded-md border border-white/10 px-1.5 py-0.5 text-[10px] text-ink-muted">
              {activeCount} hits
            </span>
          </div>
          <div className="flex flex-1 flex-col gap-5 overflow-y-auto p-4">
            {/* macros */}
            <div>
              <div className="mb-3.5 text-[10px] font-semibold uppercase tracking-wide text-ink-dim">Channel</div>
              <div className="flex justify-between">
                {macros.map((m) => (
                  <Knob key={m.label} value={m.v} size={46} color={current.color} label={m.label} display={m.value} />
                ))}
              </div>
            </div>
            {/* waveform */}
            <div>
              <div className="mb-2 text-[10px] font-semibold uppercase tracking-wide text-ink-dim">Sample</div>
              <div className="flex h-[62px] items-center overflow-hidden rounded-[9px] border border-white/[0.07] bg-sunken px-3">
                <Waveform seed={(sel + 1) * 733 + 91} color={lighten(current.color, 0.2)} className="h-[38px]" />
              </div>
            </div>
            {/* velocity graph */}
            <div>
              <div className="mb-2.5 flex items-center justify-between">
                <span className="text-[10px] font-semibold uppercase tracking-wide text-ink-dim">Velocity / Step</span>
                <span className="text-[10px] text-ink-muted">Graph editor</span>
              </div>
              <div className="flex h-[84px] items-end gap-2.5 rounded-[9px] border border-white/[0.07] bg-sunken px-2.5 pb-2.5 pt-2.5">
                {Array.from({ length: 4 }, (_, g) => (
                  <div key={g} className="flex h-full flex-1 items-end gap-1">
                    {Array.from({ length: 4 }, (_, k) => {
                      const si = g * 4 + k;
                      const stp = current.steps[si];
                      const isPlay = playing && si === playStep;
                      const h = stp.on ? 16 + stp.vel * 84 : 7;
                      return (
                        <div key={k} className="flex h-full flex-1 flex-col justify-end">
                          <div
                            className="w-full rounded-sm"
                            style={{
                              height: `${h}%`,
                              background: stp.on
                                ? isPlay
                                  ? lighten(current.color, 0.35)
                                  : current.color
                                : "rgba(255,255,255,0.08)",
                              boxShadow: stp.on ? `0 0 6px ${rgba(current.color, 0.4)}` : undefined,
                            }}
                          />
                        </div>
                      );
                    })}
                  </div>
                ))}
              </div>
            </div>
            {/* piano roll bridge */}
            <button
              type="button"
              onClick={onPianoRoll}
              className="flex items-center gap-2.5 rounded-[11px] border border-accent/20 bg-accent/[0.06] px-3 py-2.5 text-left"
            >
              <span className="flex h-7 w-7 flex-none items-center justify-center rounded-lg bg-accent/15">
                <Piano size={15} className="text-accent" />
              </span>
              <div className="flex-1">
                <div className="text-[12px] font-medium text-ink-secondary">Edit in Piano Roll</div>
                <div className="text-[10.5px] text-ink-muted">Draw melodies &amp; chords</div>
              </div>
              <span className="text-[14px] text-accent">→</span>
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
