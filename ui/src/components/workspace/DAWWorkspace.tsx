import { useMemo, useState } from "react";
import { TopBar, ModeNav, StatusBar } from "@/components/chrome";
import { Browser } from "@/components/browser";
import { Arrangement, SessionGrid } from "@/components/arrangement";
import { Inspector, Spotlight } from "@/components/ai";
import { MixConsole } from "@/components/mixer";
import { DeviceChain } from "@/components/devicechain";
import { ClipEditorModal } from "@/components/editor";
import { project } from "@/lib/project";
import type { Clip, ClipEditorMode, WorkspaceView } from "@/lib/types";

function viewFromHash(): WorkspaceView {
  const h = window.location.hash.replace("#", "");
  return h === "mix" || h === "split" || h === "session" ? (h as WorkspaceView) : "arrange";
}

/** Single-window DAW shell. Owns view + AI mode + device-drawer state and
   composes the leaf panels into the Arrange / Mix / Split / Session layouts.
   Initial view (and spotlight) can be deep-linked via the URL hash. */
export function DAWWorkspace() {
  const [view, setView] = useState<WorkspaceView>(viewFromHash);
  const [spotlightOpen, setSpotlightOpen] = useState(
    () => window.location.hash === "#spotlight",
  );
  const [deviceTrackId, setDeviceTrackId] = useState<string | null>(
    () => project.tracks.find((t) => t.devices.length > 0)?.id ?? null,
  );
  const [selectedClipId, setSelectedClipId] = useState<string | null>(null);
  const [editorClip, setEditorClip] = useState<Clip | null>(null);
  const [editorMode, setEditorMode] = useState<ClipEditorMode>("piano");

  const deviceTrack = useMemo(
    () => project.tracks.find((t) => t.id === deviceTrackId) ?? null,
    [deviceTrackId],
  );

  // Single click selects a clip; double-clicking a MIDI clip opens the editor.
  // TODO: route selection through engine.send({ type: "clip.select" }) once the
  // UI subscribes to the engine instead of the static fixture.
  const handleSelectClip = (clip: Clip) => setSelectedClipId(clip.id);
  const handleOpenClip = (clip: Clip) => {
    setSelectedClipId(clip.id);
    setEditorMode("piano");
    setEditorClip(clip);
  };

  const modeLabel =
    view === "mix"
      ? "Mix console"
      : view === "split"
        ? "Split view"
        : view === "session"
          ? "Session"
          : spotlightOpen
            ? "Command palette"
            : "Assistant docked";

  return (
    <div className="flex h-screen w-screen flex-col overflow-hidden bg-background text-foreground">
      <TopBar project={project} view={view} onAssistant={() => setSpotlightOpen(true)} />
      <ModeNav view={view} onViewChange={setView} />

      <main className="flex min-h-0 flex-1 flex-col">
        {view === "arrange" && (
          <div className="flex min-h-0 flex-1">
            <Browser project={project} />
            <div className="flex min-h-0 min-w-0 flex-1 flex-col">
              <Arrangement
                project={project}
                onOpenDevices={setDeviceTrackId}
                selectedClipId={selectedClipId}
                onSelectClip={handleSelectClip}
                onOpenClip={handleOpenClip}
              />
              {deviceTrack && (
                <div className="h-[196px] shrink-0 border-t border-border">
                  <DeviceChain track={deviceTrack} onClose={() => setDeviceTrackId(null)} />
                </div>
              )}
            </div>
            <Inspector project={project} />
          </div>
        )}

        {view === "mix" && <MixConsole project={project} />}

        {view === "split" && (
          <div className="flex min-h-0 min-w-0 flex-1 flex-col">
            <div className="min-h-0 min-w-0 flex-1">
              <Arrangement
                project={project}
                onOpenDevices={setDeviceTrackId}
                selectedClipId={selectedClipId}
                onSelectClip={handleSelectClip}
                onOpenClip={handleOpenClip}
              />
            </div>
            <div className="h-[320px] shrink-0 border-t border-border">
              <MixConsole project={project} compact />
            </div>
          </div>
        )}

        {view === "session" && (
          <div className="flex min-h-0 flex-1">
            <Browser project={project} />
            <SessionGrid project={project} />
            <Inspector project={project} />
          </div>
        )}
      </main>

      <StatusBar project={project} modeLabel={modeLabel} />
      <Spotlight open={spotlightOpen} onClose={() => setSpotlightOpen(false)} project={project} />
      <ClipEditorModal
        clip={editorClip}
        mode={editorMode}
        onModeChange={setEditorMode}
        onClose={() => setEditorClip(null)}
      />
    </div>
  );
}
