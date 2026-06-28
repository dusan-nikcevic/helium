import { Button } from "@heroui/react";

export function App() {
  return (
    <div className="flex h-screen flex-col items-center justify-center gap-4 bg-background text-foreground">
      <h1 className="text-2xl font-semibold tracking-tight">Aria — Zephyr DAW</h1>
      <p className="text-muted text-sm">Scaffold online. Engine: libardour · 48 kHz / 24-bit</p>
      <Button>Boot session</Button>
    </div>
  );
}
