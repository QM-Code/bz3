# src/game/ui/frontends/imgui/AGENTS.md

Read `src/game/ui/frontends/AGENTS.md` first.
This directory implements the **ImGui-based UI frontend**.

## Structure
- `backend.*`
  - ImGui frame lifecycle and input handling.
  - Renders HUD then console.

- `console/`
  - Console panels and tab handling (community/settings/bindings/etc).

- `hud/`
  - HUD widgets (chat, radar, scoreboard, crosshair, fps).

## Integration
- Uses engine ImGui render bridge for render-to-texture.
- Consumes models from `src/game/ui/models/` and controllers.

## Gotchas
- Input capture rules are subtle (e.g., in-game text boxes losing focus).
- Keep parity with RmlUi features.
