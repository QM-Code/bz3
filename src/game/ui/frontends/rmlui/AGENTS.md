# src/game/ui/frontends/rmlui/AGENTS.md

Read `src/game/ui/frontends/AGENTS.md` first.
This directory implements the **RmlUi-based UI frontend**.

## Structure
- `backend.*`
  - RmlUi context creation and lifecycle.
  - Loads HUD and console documents from `data/client/ui/`.

- `console/`
  - Panel system; each panel is a class (`panel_*`).
  - RML/RCSS templates live under `data/client/ui`.

- `hud/`
  - RmlUi HUD document and helpers.

## Integration
- Uses engine RmlUi render interfaces (bgfx/diligent).
- Consumes models from `src/game/ui/models/`.

## Gotchas
- Pay attention to render scale and DPI scaling.
- Keep parity with ImGui features and behavior.
