# src/engine/physics/backends/AGENTS.md

Read `src/engine/physics/AGENTS.md` first.
This directory contains backend-specific physics implementations.

## Backends
- `jolt/`
- `physx/`

## Scope
Each backend implements the engine’s physics interfaces:
- World simulation
- Rigid bodies
- Static bodies
- Player controller

## Selection
Backend is selected at build time via `KARMA_PHYSICS_BACKEND`.
