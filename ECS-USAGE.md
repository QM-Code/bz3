# ECS Usage Notes (Karma/BZ3)

This document captures the current stance on ECS usage in this project and when it makes sense to use it. It is intended for future agents and contributors.

## What ECS is (short version)

ECS (Entity‑Component‑System) models game objects as:
- **Entities**: IDs (no behavior).
- **Components**: plain data attached to entities (position, mesh, health).
- **Systems**: logic that iterates over component sets to update behavior.

It’s useful for batch‑processing many similar objects and keeping data/behavior decoupled.

## Current usage in this codebase

- ECS is **used for rendering** in the engine:
  - `ecs::World`, components, and `RenderSystem` live under `src/engine/ecs/`.
  - `karma::app::EngineApp` owns the ECS world and runs the render system each frame.
  - Game adapters feed ECS render components from gameplay objects.

- ECS is **not used for physics**:
  - Physics remains in the engine’s physics system (Jolt/PhysX backends).
  - No physics update loop is driven by ECS.

## When ECS is a good fit here

- Rendering, visual effects, and other “many instances of similar data” tasks.
- Systems that benefit from data‑oriented iteration (e.g., large numbers of projectiles).
- Engine‑level systems that should remain game‑agnostic and cleanly decoupled.

## When ECS is *not* a good fit here (right now)

- Gameplay logic already modeled as objects (tanks, shots, sessions) with clear behavior.
- Physics integration where existing engine systems already handle ownership and lifecycle.
- Areas where ECS adds indirection without clear performance or clarity benefits.

## Recommendation (current stance)

- **Keep ECS for rendering** (already in place and useful).
- **Do not migrate physics into ECS** unless there is a clear, concrete benefit.
- Use ECS *selectively* where it simplifies or speeds up the implementation.

## Optional conservative pattern

If physics data is needed in ECS, prefer **read‑only bridge components**:
- Let physics remain the source of truth.
- Mirror position/velocity into ECS components for rendering or UI queries.
- Avoid driving physics updates through ECS unless required.
