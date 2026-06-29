import { useCallback, useEffect, useLayoutEffect, useRef, useState } from "react";
import { Button } from "@heroui/react";
import { Plus, Maximize2 } from "lucide-react";
import { cn } from "@/lib/cn";
import type { Clip, Project } from "@/lib/types";
import { ClipView } from "./ClipView";
import { LANE_H, PX_PER_BAR, clampZoom } from "./constants";
import { TrackHeader } from "./TrackHeader";

interface ArrangementProps {
  project: Project;
  onOpenDevices?: (trackId: string) => void;
  selectedClipId?: string | null;
  onSelectClip?: (clip: Clip) => void;
  onOpenClip?: (clip: Clip) => void;
}

const PLAYHEAD_BAR = 17.1;
const HIDE_SCROLLBAR = "[scrollbar-width:none] [&::-webkit-scrollbar]:hidden";
/** Below this zoom, beat subdivisions become visual noise — show bar lines only. */
const BEAT_LINES_MIN_PX = 22;
/** Multiplicative step for keyboard +/- zoom. */
const KEY_ZOOM_STEP = 1.2;

/**
 * The center arrangement: a frozen left track-header column aligned with a
 * scrollable timeline (section markers, bar ruler and per-track clip lanes).
 * The timeline zooms horizontally via Ctrl/Cmd + scroll (anchored on the
 * cursor), keyboard +/-, and zoom-to-fit (Shift+Z / the Fit button).
 */
export function Arrangement({
  project,
  onOpenDevices,
  selectedClipId,
  onSelectClip,
  onOpenClip,
}: ArrangementProps) {
  const topRef = useRef<HTMLDivElement>(null);
  const leftRef = useRef<HTMLDivElement>(null);
  const lanesRef = useRef<HTMLDivElement>(null);

  const { startBar, totalBars, sections, tracks } = project;

  const [pxPerBar, setPxPerBar] = useState<number>(PX_PER_BAR);
  // current zoom mirrored into a ref so the once-bound wheel/key listeners read
  // the latest value without re-binding
  const pxRef = useRef(pxPerBar);
  pxRef.current = pxPerBar;
  // scrollLeft to apply once the new width has laid out (keeps the anchor fixed)
  const pendingScroll = useRef<number | null>(null);

  const contentW = totalBars * pxPerBar;
  const lanesH = tracks.length * LANE_H;

  // keep the section/ruler row and the header column locked to the lanes scroll
  const syncScroll = useCallback(() => {
    const lanes = lanesRef.current;
    if (!lanes) return;
    if (topRef.current) topRef.current.scrollLeft = lanes.scrollLeft;
    if (leftRef.current) leftRef.current.scrollTop = lanes.scrollTop;
  }, []);

  // Zoom so the musical position under `anchorClientX` stays put on screen.
  const zoomAround = useCallback((nextPx: number, anchorClientX: number) => {
    const lanes = lanesRef.current;
    if (!lanes) return;
    const next = clampZoom(nextPx);
    if (next === pxRef.current) return;
    const rect = lanes.getBoundingClientRect();
    const offset = anchorClientX - rect.left;
    const barAtCursor = (lanes.scrollLeft + offset) / pxRef.current;
    pendingScroll.current = barAtCursor * next - offset;
    setPxPerBar(next);
  }, []);

  const fitToWidth = useCallback(() => {
    const lanes = lanesRef.current;
    if (!lanes) return;
    pendingScroll.current = 0;
    setPxPerBar(clampZoom(lanes.clientWidth / totalBars));
  }, [totalBars]);

  // apply the anchored scrollLeft after the zoomed width has laid out
  useLayoutEffect(() => {
    const lanes = lanesRef.current;
    if (lanes && pendingScroll.current != null) {
      lanes.scrollLeft = Math.max(0, pendingScroll.current);
      pendingScroll.current = null;
      syncScroll();
    }
  }, [pxPerBar, syncScroll]);

  // Ctrl/Cmd + wheel (and trackpad pinch) — native non-passive so preventDefault works
  useEffect(() => {
    const el = lanesRef.current;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      if (!(e.ctrlKey || e.metaKey)) return;
      e.preventDefault();
      zoomAround(pxRef.current * Math.exp(-e.deltaY * 0.0015), e.clientX);
    };
    el.addEventListener("wheel", onWheel, { passive: false });
    return () => el.removeEventListener("wheel", onWheel);
  }, [zoomAround]);

  // keyboard: +/- zoom around viewport center, Shift+Z zoom-to-fit
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const t = e.target as HTMLElement | null;
      if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) return;
      const lanes = lanesRef.current;
      if (!lanes) return;
      const center = lanes.getBoundingClientRect().left + lanes.clientWidth / 2;
      if (e.key === "+" || e.key === "=") {
        e.preventDefault();
        zoomAround(pxRef.current * KEY_ZOOM_STEP, center);
      } else if (e.key === "-" || e.key === "_") {
        e.preventDefault();
        zoomAround(pxRef.current / KEY_ZOOM_STEP, center);
      } else if ((e.key === "z" || e.key === "Z") && e.shiftKey) {
        e.preventDefault();
        fitToWidth();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [zoomAround, fitToWidth]);

  const chorus = sections.find((s) => s.highlight);
  const bars = Array.from({ length: totalBars + 1 }, (_, i) => i);
  const playheadX = (PLAYHEAD_BAR - startBar) * pxPerBar;
  const showBeatLines = pxPerBar >= BEAT_LINES_MIN_PX;

  return (
    <div className="flex min-h-0 flex-1 flex-col overflow-hidden bg-panel">
      {/* ----- section markers + ruler (frozen vertically) ----- */}
      <div className="flex flex-none">
        {/* top-left corner cells */}
        <div className="w-[150px] flex-none border-r border-white/5">
          <div className="h-6 border-b border-white/5" />
          <div className="flex h-6 items-center gap-1 border-b border-white/10 bg-rail px-2.5">
            <span className="text-[8px] font-semibold uppercase tracking-wide text-ink-muted">Bars</span>
            <span className="font-mono text-[8px] text-ink-faint tabular-nums">
              {Math.round((pxPerBar / PX_PER_BAR) * 100)}%
            </span>
            <div className="flex-1" />
            <button
              type="button"
              onClick={fitToWidth}
              title="Zoom to fit (Shift+Z)"
              className="flex h-4 w-4 items-center justify-center rounded text-ink-muted hover:bg-white/10 hover:text-ink"
            >
              <Maximize2 className="h-2.5 w-2.5" />
            </button>
          </div>
        </div>
        {/* scrollable section + ruler */}
        <div ref={topRef} className={cn("relative min-w-0 flex-1 overflow-hidden", HIDE_SCROLLBAR)}>
          <div style={{ width: `${contentW}px` }}>
            {/* section markers */}
            <div className="relative h-6 border-b border-white/5">
              {sections.map((sec) => {
                const left = (sec.startBar - startBar) * pxPerBar;
                const width = sec.lengthBars * pxPerBar;
                return (
                  <div
                    key={sec.id}
                    className={cn(
                      "absolute top-1 flex h-4 items-center justify-center rounded-[5px] text-[10px] font-semibold uppercase tracking-wide",
                      sec.highlight ? "bg-accent/15 text-accent" : "bg-white/5 text-ink-muted",
                    )}
                    style={{ left: `${left + 3}px`, width: `${width - 6}px` }}
                  >
                    {sec.name}
                  </div>
                );
              })}
            </div>
            {/* ruler */}
            <div className="relative h-6 border-b border-white/10 bg-rail">
              {bars.slice(0, totalBars).map((b) => (
                <span
                  key={b}
                  className="absolute top-1.5 font-mono text-[10px] text-ink-faint"
                  style={{ left: `${b * pxPerBar + 6}px` }}
                >
                  {startBar + b}
                </span>
              ))}
            </div>
          </div>
        </div>
      </div>

      {/* ----- track headers + clip lanes (shared vertical scroll) ----- */}
      <div className="flex min-h-0 flex-1">
        {/* frozen header column */}
        <div
          ref={leftRef}
          className={cn(
            "w-[150px] flex-none overflow-hidden border-r border-white/5",
            HIDE_SCROLLBAR,
          )}
        >
          {tracks.map((track) => (
            <TrackHeader key={track.id} track={track} onOpenDevices={onOpenDevices} />
          ))}
          <Button
            variant="ghost"
            size="sm"
            fullWidth
            className="h-10 justify-start gap-2 rounded-none px-3 text-[11px] text-ink-muted"
            onPress={() => {}}
          >
            <Plus className="h-3.5 w-3.5" />
            Add Track
          </Button>
        </div>

        {/* scrollable lanes */}
        <div
          ref={lanesRef}
          onScroll={syncScroll}
          className={cn("relative min-w-0 flex-1 overflow-auto", HIDE_SCROLLBAR)}
        >
          <div className="relative" style={{ width: `${contentW}px`, height: `${lanesH}px` }}>
            {/* chorus highlight overlay */}
            {chorus && (
              <div
                className="pointer-events-none absolute inset-y-0 z-0 border-l border-accent/25 bg-accent/[0.04]"
                style={{
                  left: `${(chorus.startBar - startBar) * pxPerBar}px`,
                  width: `${chorus.lengthBars * pxPerBar}px`,
                }}
              />
            )}

            {/* bar + beat gridlines */}
            {bars.map((b) => (
              <div
                key={`bar-${b}`}
                className="absolute inset-y-0 z-0 border-l border-white/5"
                style={{ left: `${b * pxPerBar}px` }}
              />
            ))}
            {showBeatLines &&
              bars.slice(0, totalBars).map((b) =>
                [1, 2, 3].map((k) => (
                  <div
                    key={`beat-${b}-${k}`}
                    className="absolute inset-y-0 z-0 border-l border-white/[0.02]"
                    style={{ left: `${b * pxPerBar + (k * pxPerBar) / 4}px` }}
                  />
                )),
              )}

            {/* one lane per track */}
            {tracks.map((track) => (
              <div
                key={track.id}
                className="relative z-10 border-b border-white/5"
                style={{ height: `${LANE_H}px` }}
              >
                {track.clips.map((clip) => (
                  <ClipView
                    key={clip.id}
                    clip={clip}
                    pxPerBar={pxPerBar}
                    selected={selectedClipId ? clip.id === selectedClipId : undefined}
                    onSelect={onSelectClip}
                    onOpen={onOpenClip}
                  />
                ))}
              </div>
            ))}

            {/* playhead */}
            <div
              className="pointer-events-none absolute inset-y-0 z-20 w-px bg-accent"
              style={{ left: `${playheadX}px` }}
            >
              <div className="absolute -left-[3px] top-0 h-0 w-0 border-l-[4px] border-r-[4px] border-t-[6px] border-l-transparent border-r-transparent border-t-accent" />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
