import { Waveform } from "@/components/primitives";
import { cn } from "@/lib/cn";
import type { Clip, MidiNote } from "@/lib/types";
import { TRACK_HEX } from "@/lib/types";
import { ORIGIN_BAR, PX_PER_BAR, withAlpha } from "./constants";

const HEADER_H = 13;

const NOTE_ROWS = 12;

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

/** Map a note's pitch into one of NOTE_ROWS lanes within the clip body. */
function pitchRow(note: MidiNote, min: number, span: number): number {
  const normalized = span === 0 ? 0.5 : (note.pitch - min) / span;
  return Math.round((1 - normalized) * (NOTE_ROWS - 1));
}

/** Mini note blocks inside a midi clip (design genNotes look — short colored
   bars about one lane tall, fill lightened, hairline ring). */
function MidiNotes({ clip, pxPerBar }: { clip: Extract<Clip, { type: "midi" }>; pxPerBar: number }) {
  const hex = TRACK_HEX[clip.color];
  const fill = lighten(hex, 0.1);
  const pitches = clip.notes.map((n) => n.pitch);
  const min = pitches.length ? Math.min(...pitches) : 0;
  const max = pitches.length ? Math.max(...pitches) : 1;
  const span = max - min;
  return (
    <div className="absolute inset-0">
      {clip.notes.map((n, i) => (
        <div
          key={i}
          className="absolute rounded-[1.5px]"
          style={{
            left: `${(n.bar + n.beat / 4) * pxPerBar}px`,
            width: `${Math.max(3, (n.length / 4) * pxPerBar - 2)}px`,
            top: `${(pitchRow(n, min, span) / NOTE_ROWS) * 100}%`,
            height: `${(1 / NOTE_ROWS) * 92}%`,
            background: fill,
            boxShadow: `inset 0 0 0 0.5px ${withAlpha(hex, 0.65)}`,
          }}
        />
      ))}
    </div>
  );
}

interface ClipViewProps {
  clip: Clip;
  /** horizontal scale (pixels per bar) — driven by the timeline zoom */
  pxPerBar?: number;
  selected?: boolean;
  /** single click — selects the clip */
  onSelect?: (clip: Clip) => void;
  /** double click — opens the clip editor (midi only) */
  onOpen?: (clip: Clip) => void;
}

/**
 * A single clip rendered absolutely inside its lane. Audio clips draw a
 * procedural waveform; midi clips draw a tiny note grid. Position and width
 * derive from the clip's bar geometry. Single click selects; double-clicking a
 * MIDI clip opens the piano-roll / pattern editor.
 */
export function ClipView({ clip, pxPerBar = PX_PER_BAR, selected, onSelect, onOpen }: ClipViewProps) {
  const hex = TRACK_HEX[clip.color];
  const left = (clip.startBar - ORIGIN_BAR) * pxPerBar + 1;
  const width = clip.lengthBars * pxPerBar - 2;
  const isSelected = selected ?? clip.selected;
  const isMidi = clip.type === "midi";

  return (
    <div
      onClick={() => onSelect?.(clip)}
      onDoubleClick={() => {
        if (isMidi) onOpen?.(clip);
      }}
      className={cn(
        "absolute overflow-hidden rounded-[3px] border",
        isMidi && "cursor-pointer",
        isSelected ? "border-accent shadow-[0_0_0_3px_rgba(41,151,255,0.18)]" : "border-white/10",
      )}
      style={{
        left: `${left}px`,
        width: `${width}px`,
        top: "3px",
        bottom: "3px",
        background: withAlpha(hex, 0.18),
      }}
    >
      <div
        className="flex items-center truncate px-1.5 text-[9px] font-semibold tracking-[0.2px]"
        style={{ height: `${HEADER_H}px`, background: hex, color: "#17171b" }}
      >
        <span className="truncate">{clip.name}</span>
      </div>
      <div className="relative w-full" style={{ height: `calc(100% - ${HEADER_H}px)` }}>
        {clip.type === "audio" ? (
          <Waveform seed={clip.seed} color={lighten(hex, 0.3)} />
        ) : (
          <MidiNotes clip={clip} pxPerBar={pxPerBar} />
        )}
      </div>
    </div>
  );
}
