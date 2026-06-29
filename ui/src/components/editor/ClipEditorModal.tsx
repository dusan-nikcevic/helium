import { useEffect } from "react";
import { X } from "lucide-react";
import { Segmented, type SegmentedOption } from "@/components/primitives";
import type { Clip, ClipEditorMode } from "@/lib/types";
import { PianoRoll } from "./PianoRoll";
import { PatternDesigner } from "./PatternDesigner";

const MODE_OPTS: SegmentedOption<ClipEditorMode>[] = [
  { key: "piano", label: "Piano Roll" },
  { key: "pattern", label: "Pattern" },
];

interface ClipEditorModalProps {
  clip: Clip | null;
  mode: ClipEditorMode;
  onModeChange: (mode: ClipEditorMode) => void;
  onClose: () => void;
}

/** Floating clip-detail editor. Opens over a dimmed workspace when a MIDI clip
   is double-clicked; toggles between the Piano Roll and Pattern Designer. */
export function ClipEditorModal({ clip, mode, onModeChange, onClose }: ClipEditorModalProps) {
  useEffect(() => {
    if (!clip) return;
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") onClose();
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [clip, onClose]);

  if (!clip) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center">
      <div className="absolute inset-0 bg-black/[0.58] backdrop-blur-sm" onClick={onClose} />
      <div
        onClick={(event) => event.stopPropagation()}
        className="relative flex h-[min(760px,90vh)] w-[min(1100px,94vw)] flex-col overflow-hidden rounded-[18px] border border-white/[0.14] bg-overlay/90 shadow-[0_40px_90px_rgba(0,0,0,0.55)] backdrop-blur-2xl"
      >
        {/* header */}
        <div className="flex h-12 flex-none items-center gap-3 border-b border-white/[0.08] bg-rail px-4">
          <Segmented options={MODE_OPTS} value={mode} onChange={onModeChange} size="sm" />
          <span className="h-[18px] w-px bg-white/10" />
          <span className="text-[12.5px] text-ink-secondary">
            {clip.name}
            <span className="ml-2 text-ink-dim">· {clip.type === "midi" ? "MIDI clip" : "Audio clip"}</span>
          </span>
          <div className="flex-1" />
          <button
            type="button"
            onClick={onClose}
            className="flex h-6 w-6 items-center justify-center rounded-md bg-white/[0.06] text-ink-secondary hover:bg-white/10"
          >
            <X size={14} />
          </button>
        </div>
        {/* body */}
        <div className="min-h-0 flex-1">
          {mode === "piano" ? (
            <PianoRoll clip={clip} subtitle={`${clip.name} · Chorus`} onChannelRack={() => onModeChange("pattern")} />
          ) : (
            <PatternDesigner onPianoRoll={() => onModeChange("piano")} />
          )}
        </div>
      </div>
    </div>
  );
}
