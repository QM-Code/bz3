# src/game/renderer/AGENTS.md

Read `src/game/AGENTS.md` first.
This directory holds **game-specific rendering orchestration**.

## Scope
- Ties game entities to engine renderer core.
- Manages game render IDs and ECS entity registration.
- Owns radar rendering (offscreen target + overlays).

## Key files
- `renderer.*`
  - Game-facing renderer wrapper.
  - Creates entities, sets meshes/materials, updates transforms.
  - Controls camera and UI overlay composition.

- `radar_renderer.*`
  - Radar render target, radar objects (player/shot/buildings), FOV lines.
  - Handles radar orientation and world-to-radar mapping.

## How it connects
- Uses engine renderer core (`src/engine/renderer/`).
- Uses engine graphics backends for GPU resources.
- HUD uses radar texture from here.

## Gotchas
- Radar is sensitive to orientation math; test in all backends after changes.
- UI overlay composition is done by the game renderer, not engine.
