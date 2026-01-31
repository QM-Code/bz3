# src/engine/ecs/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory contains the **engine ECS scaffolding**: lightweight entity
registries, components, and optional systems.

## Purpose
ECS is **optional**. The engine can render and simulate without it, but the
scaffolding exists to support more data-driven game logic later.

## Key pieces
- `world.hpp`
  - Owns entity registry and component stores.
- `components.hpp`
  - Common component types (transform, render mesh, etc.).
- `registry.hpp`
  - Generic storage for component types.
- `systems/`
  - Optional engine-level systems (e.g., renderer integration).

## How the game uses it
- The game renderer may register ECS entities for rendering.
- Game code is free to ignore ECS entirely.

## Gotchas
- Keep ECS generic and engine-level. Avoid game-specific components here.
