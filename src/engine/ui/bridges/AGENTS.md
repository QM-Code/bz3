# src/engine/ui/bridges/AGENTS.md

Read `src/engine/ui/AGENTS.md` first.
This directory defines **bridge interfaces** between UI frameworks and graphics
backends.

## Purpose
- Normalize how UI output textures are exposed to the game renderer.
- Keep UI frameworks (ImGui/RmlUi) backend-agnostic.

## Typical flow
UI renderer → bridge → graphics backend → game renderer composite.
