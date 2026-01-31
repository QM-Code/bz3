# src/engine/ecs/systems/AGENTS.md

Read `src/engine/ecs/AGENTS.md` first.
This directory contains **optional ECS systems** that operate on engine ECS
components.

## Current systems
- `renderer_system.*`
  - Bridges ECS render components to the engine renderer.

## When to use
- If the game adopts ECS for rendering or simulation, systems here can be
  extended or replaced.

## Caution
Do not add game-specific systems here. Keep systems engine-generic and reusable.
