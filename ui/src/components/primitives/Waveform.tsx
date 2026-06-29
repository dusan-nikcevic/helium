import { useMemo } from "react";
import { cn } from "@/lib/cn";

interface WaveformProps {
  /** deterministic seed so the same clip always renders identically */
  seed: number;
  color?: string;
  /** number of amplitude samples — higher is smoother/more granular */
  samples?: number;
  className?: string;
}

const VW = 1000;
const VH = 100;
const MID = VH / 2;

/** mulberry32 — matches the design's RNG so waveforms read identically. */
function makeRng(seed: number) {
  let t = seed >>> 0;
  return () => {
    t += 0x6d2b79f5;
    let r = Math.imul(t ^ (t >>> 15), 1 | t);
    r ^= r + Math.imul(r ^ (r >>> 7), 61 | r);
    return ((r ^ (r >>> 14)) >>> 0) / 4294967296;
  };
}

/** Procedural audio waveform drawn as a smooth filled, mirrored SVG path
   (port of the design's wavePath) — fills its container. */
export function Waveform({ seed, color = "currentColor", samples = 240, className }: WaveformProps) {
  const path = useMemo(() => {
    const rng = makeRng(seed);
    const n = Math.max(16, samples);
    const amps: number[] = [];
    for (let i = 0; i < n; i++) {
      // sine envelope so clips taper at the edges, squared noise for spikiness
      const env = Math.pow(Math.sin((Math.PI * i) / (n - 1)), 0.5);
      let a = 0.2 + 0.8 * rng();
      a = a * a;
      amps.push((0.08 + a * 0.92) * MID * env);
    }
    const x = (i: number) => ((i / (n - 1)) * VW).toFixed(1);
    let d = `M0 ${MID}`;
    for (let i = 0; i < n; i++) d += ` L${x(i)} ${(MID - amps[i]).toFixed(1)}`;
    for (let i = n - 1; i >= 0; i--) d += ` L${x(i)} ${(MID + amps[i]).toFixed(1)}`;
    return `${d}Z`;
  }, [seed, samples]);

  return (
    <svg
      viewBox={`0 0 ${VW} ${VH}`}
      preserveAspectRatio="none"
      className={cn("h-full w-full", className)}
    >
      <path d={path} fill={color} />
    </svg>
  );
}
