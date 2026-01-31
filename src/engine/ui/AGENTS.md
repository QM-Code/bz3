# src/engine/ui/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory provides **engine-level UI bridges and platform renderers**.

## Purpose
- Engine owns the render-to-texture bridge for UI frameworks.
- Provides backend-specific render interfaces for RmlUi and ImGui.
- Game UI frontends live in `src/game/ui/` and use these bridges.

## Subdirectories
- `bridges/` — backend-agnostic interfaces between UI and graphics backends.
- `platform/` — RmlUi render interfaces per backend (bgfx/diligent).
- `imgui/` — helper wrappers for ImGui integration.

## How it connects to game UI
- Game frontends call into engine bridges to obtain output textures.
- Engine graphics backends provide backend-specific handles.

## Gotchas
- Keep engine UI bridge code free of game logic.
- UI output must be compatible across bgfx/diligent.
