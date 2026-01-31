# src/game/ui/core/AGENTS.md

Read `src/game/ui/AGENTS.md` first.
This directory contains the **UI system core** shared by both frontends.

## Responsibilities
- Own the selected UI backend (ImGui or RmlUi).
- Drive per-frame UI updates.
- Apply config-driven HUD visibility changes.
- Expose render output to the renderer.

## Key files
- `system.*` — `UiSystem` lifecycle and update loop.
- `backend.hpp` — backend interface implemented by ImGui and RmlUi.
- `backend_factory.cpp` — chooses backend at build time.

## Integration points
- Renderer pulls UI render output and composites it.
- Input events are forwarded to backend-specific UI systems.
