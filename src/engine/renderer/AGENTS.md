# src/engine/renderer/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory defines the **engine renderer core** that orchestrates render
passes independent of the game layer.

## Purpose
- Provide a renderer that can be fed by game-level logic.
- Own render contexts (camera, layers, lighting, targets).
- Expose a stable API to game renderers.

## Key files
- `renderer_core.*`
  - Core render loop orchestration.

- `renderer_context.*`
  - Per-frame render state (camera, layers, lighting).

- `scene_renderer.*`
  - Scene-level rendering wrapper that draws entities using graphics backend.

## How it connects to game code
- Game renderer holds a `RendererCore` instance and controls what to render.
- Game sets camera transforms and main layer in the context.
- UI overlays are composed after scene render via bridges.

## Common tasks
- Add a render pass: update renderer core + graphics backend if needed.
- Debug draw order: check renderer core sequence.
