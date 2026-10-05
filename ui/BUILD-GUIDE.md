# Aria React reference UI build guide

This guide applies to the React reference app. The production UI choice is Qt Quick/QML and C++.
See [the native UI guide](../native/README.md) for the runnable desktop implementation.
The HTML files in `design-ref/` define the visual contract for both implementations.

## Stack & hard rules

- **bun** for everything (`bun add`, `bun run`). React **19**, TypeScript **strict**, Vite, Tailwind **v4**.
- **HeroUI v3 OSS** (`@heroui/react`) for standard components. No Provider. **Compound components**.
  HeroUI interactive elements use **`onPress`**, NOT `onClick`. Button uses **`variant`**, there is **no `color` prop**.
- **Tailwind utilities only.** Do **not** create `.css` files or write custom CSS classes. The only
  stylesheet is `src/styles/theme.css` (already done — do not edit it). Inline `style={{...}}` is
  allowed **only** for runtime-computed geometry that cannot be a static class (gradient angles,
  fader/meter positions, SVG paths, dynamic colors from data). Everything static = Tailwind classes.
- Custom (non-HeroUI) elements use native `onClick`.
- Path alias: **`@` → `src`** (e.g. `import { Knob } from "@/components/primitives"`).
- Accessibility: give icon-only buttons `aria-label`. Use semantic elements.

## File ownership (CRITICAL for parallel work)

- Put **all** your files under your assigned folder, e.g. `src/components/<your-area>/`.
- Export your top-level component(s) with the **exact names/signatures** listed in "Panel contracts".
- **Do NOT edit** any of these shared files (the orchestrator owns them): `src/App.tsx`,
  `src/lib/*`, `src/components/primitives/*`, `src/styles/theme.css`, `index.html`,
  `package.json`, `vite.config.ts`, `tsconfig.json`, `src/components/workspace/*`.
- Do **not** run `bun install` / add dependencies. Everything you need is installed
  (`@heroui/react`, `@heroui/styles`, `lucide-react`, `clsx`, `tailwind-merge`).
- Import shared code only: primitives from `@/components/primitives`, types from `@/lib/types`,
  the demo session from `@/lib/project` (`import { project } from "@/lib/project"`), `cn` from `@/lib/cn`.

## Design tokens (Tailwind classes you can use)

Theme is dark, applied via `<html data-theme="aria">`. Available semantic + DAW utility classes:

**Surfaces (dark→light):** `bg-well` `#0c0c0e` · `bg-sunken` `#0e0e10` · `bg-rail` `#121214` ·
`bg-panel` `#161618` · `bg-strip` `#1c1c1f` · `bg-elevated` `#26262a` · `bg-background` (root `#1b1b1d`) ·
`bg-surface` (cards `#161618`) · `bg-overlay` (popovers `#1c1c1f`).

**Text:** `text-ink` `#f5f5f7` · `text-ink-secondary` `#c5c5ca` · `text-ink-muted` `#86868b` ·
`text-ink-dim` `#6b6b70` · `text-ink-faint` `#56565b`. (`text-foreground` / `text-muted` also work.)

**Accent / status:** `accent` `#2997ff` (AI/primary), `success` `#54c98a`, `warning` `#e8c24e`,
`danger` `#ff5a4d`. Use as `bg-accent` `text-accent` `border-accent`, with opacity e.g. `bg-accent/15`.
Transport: `bg-rec` `#ff453a`, `bg-solo` `#f0c850`, `bg-mute` `#c0563f`.

**Track palette:** `track-drums` `#cf8163` · `track-bass` `#7e84c0` · `track-vocal` `#5aa9a0` ·
`track-vocaldbl` `#5d93b3` · `track-piano` `#bd9b62` · `track-pads` `#9b7cb8` · `track-guitars` `#8c9b66` ·
`track-fx` `#b66f80` · `track-master` `#9aa0a6`. (e.g. `bg-track-vocal`, `text-track-drums`.) For a color
chosen at runtime from data, use `TRACK_HEX[color]` from `@/lib/types` with inline style.

**Device-kind:** `dev-eq` `#6f9fc8` · `dev-dynamics` `#e0a955` · `dev-saturation` `#d9774f` ·
`dev-fx` `#9a7cb8` · `dev-instrument` `#5aa9a0` · `dev-utility` `#8a8f96`. Also `DEVICE_HEX` map.

**Borders/lines:** `border-border` (`rgba(255,255,255,.08)`), `border-separator`. For hairlines use
`border-white/5`..`border-white/10`.

**Type:** default font is the Apple system stack. Use **`font-mono`** for all numeric readouts
(timecode, dB, LUFS, BPM) — it carries tabular-nums automatically. Common sizes in this design are
tiny: `text-[10px]`, `text-[11px]`, `text-xs` (12), `text-[13px]`, `text-sm`. Uppercase micro-labels
use `text-[8px] uppercase tracking-wide text-ink-muted`.

**Radii:** `rounded-sm/md/lg/xl` (HeroUI scale), or arbitrary like `rounded-[10px]`. Panels/cards ≈ `rounded-xl` (12px).

## Primitives (already built — import, don't rebuild)

`import { Knob, Fader, Meter, MeterBar, SparkleBadge, LED, Pill, Segmented, Waveform } from "@/components/primitives";`

- `<Knob value={0..1} label? display? size?=28 color?="var(--color-accent)" aiEdited? onChange? />` — rotary, 270° arc.
- `<Fader value={0..1} color?="#2b8fff" onChange? />` — vertical; **fills parent height**, put it in a height-bounded box.
- `<Meter levelL levelR peakL? peakR? scale? />` — dual L/R, `scale` adds dB column. `<MeterBar level peak? />` single. Fills parent height.
- `<SparkleBadge size?=11 glow? />` — the AI ✦ mark (lucide Sparkles, accent).
- `<LED on? color?=success size?=6 />` — status dot.
- `<Pill active?>children</Pill>` — rounded chip.
- `<Segmented options={[{key,label}]} value onChange size?="md"|"sm" />` — segmented control.
- `<Waveform seed={number} color?="currentColor" bars?=96 />` — procedural audio waveform SVG, fills parent.

Icons: use **lucide-react** (`import { Play, Square, Circle, SkipBack, Search, ChevronDown, ... } from "lucide-react"`).

## Data model

Types in `@/lib/types` (Track, Bus, Group, Device, Insert, Send, KnobParam, Clip (audio|midi),
MasterChannel, ChatMessage, Diff, DiffChange, Project, etc.). The demo session is
`import { project, suggestionChips } from "@/lib/project"` — `project: Project` is the "Midnight"
session (F min, 140 BPM, 4/4, Chorus): 8 tracks, 4 buses, 4 groups, master with LUFS −9.4 / TP −1.0 /
GR −2.1, a 4-change AI diff, chat, AI history, browser instruments. Use it for all content — match the
design's exact labels/values.

## HeroUI v3 component cheat-sheet (@heroui/react)

All use `onPress` (not onClick); `className` passthrough on each sub-component; no `classNames` slot prop.
Need a component not listed? Fetch its docs from the **heroui-pro MCP**: `get_component_docs({components:[name]})`.

- **Button**: `variant`: `primary|secondary|tertiary|outline|ghost|danger` (+`danger-soft`); `size`: `sm|md|lg`;
  `isIconOnly`, `isDisabled`, `isPending`, `fullWidth`; `onPress`. No `color`.
- **Tabs**: `Tabs` > `Tabs.ListContainer` > `Tabs.List` > `Tabs.Tab id`(+`Tabs.Indicator`) ; `Tabs.Panel id`.
  `variant`: `primary|secondary`; `selectedKey`/`defaultSelectedKey`; `onSelectionChange`.
- **Slider**: `Slider` + `Label` + `Slider.Output` + `Slider.Track` > `Slider.Fill` + `Slider.Thumb`.
  `value`/`defaultValue` (array=range), `onChange`/`onChangeEnd`, `minValue`/`maxValue`/`step`, `orientation`.
- **Tooltip**: `Tooltip` (delay, closeDelay) > trigger child + `Tooltip.Content` (`showArrow`, `placement`, `offset`) > `Tooltip.Arrow`.
- **Kbd**: `Kbd` > `Kbd.Abbr keyValue="command|shift|enter|..."` + `Kbd.Content`. `variant`: `default|light`.
- **Input**: native `<input>` attrs; `variant`: `primary|secondary`; `value`/`defaultValue`; native `onChange`. (Use `SearchField` for search.)
- **Card**: `Card` (`variant`: `transparent|default|secondary|tertiary`) > `Card.Header`/`Card.Title`/`Card.Description`/`Card.Content`/`Card.Footer`.
- **Dropdown**: `Dropdown` (`isOpen`/`onOpenChange`) > trigger `Button` + `Dropdown.Popover` (`placement`) > `Dropdown.Menu` (`selectionMode`, `onAction`, `onSelectionChange`) > `Dropdown.Item id` (+`variant="danger"`).
- Other handy OSS: `Modal`, `Popover`, `Switch`, `ToggleButton`/`ToggleButtonGroup`, `Separator`, `ScrollShadow`, `Avatar`, `Badge`, `Chip`, `Meter`, `Toolbar`. (We have custom Meter/Segmented; prefer ours for DAW visuals.)

## Conventions

- Functional components, named exports. Keep components focused; split big panels into subcomponents in your folder.
- Derive everything from `project`; no network/state libs. Local `useState` for interactive bits (selected tab, open drawer).
- Match the design's density: small fonts, tight spacing, hairline borders, subtle shadows. Flag AI-edited
  items with `<SparkleBadge/>` + a faint `bg-accent/10` / `ring-1 ring-accent/20` treatment (see design).
- Read your `design-ref/*.dc.html` for exact structure, then reproduce it faithfully with the tokens above.

## Panel contracts (exact exports the orchestrator will import)

The orchestrator owns `src/components/workspace/DAWWorkspace.tsx` which composes these. Match signatures exactly.

- `components/chrome/` → `export function TopBar({ project, view, onAssistant }: {project: Project; view: WorkspaceView; onAssistant?: () => void})`,
  `export function ModeNav({ view, onViewChange }: {view: WorkspaceView; onViewChange: (v: WorkspaceView) => void})`,
  `export function StatusBar({ project, modeLabel }: {project: Project; modeLabel?: string})`. (index.ts barrel.)
- `components/browser/` → `export function Browser({ project }: {project: Project})`.
- `components/arrangement/` → `export function Arrangement({ project, onOpenDevices }: {project: Project; onOpenDevices?: (trackId: string) => void})`
  and `export function SessionGrid({ project }: {project: Project})` (clip-launcher view).
- `components/ai/` → `export function Inspector({ project }: {project: Project})` (docked assistant)
  and `export function Spotlight({ open, onClose, project }: {open: boolean; onClose: () => void; project: Project})` (⌘K palette).
- `components/mixer/` → `export function MixConsole({ project, compact }: {project: Project; compact?: boolean})`.
- `components/devicechain/` → `export function DeviceChain({ track, onClose }: {track: Track; onClose?: () => void})`.

Import `WorkspaceView`, `Project`, `Track` from `@/lib/types`.
