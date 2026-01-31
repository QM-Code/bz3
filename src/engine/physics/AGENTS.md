# src/engine/physics/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory implements the **physics abstraction layer** and backend
selection.

## Backends
- `jolt/`
- `physx/`

Select backend via `KARMA_PHYSICS_BACKEND` in CMake.

## Key files
- `physics_world.*`
  - Backend-agnostic physics world wrapper.

- `rigid_body.*`, `static_body.*`, `player_controller.*`
  - Engine-facing physics objects.

- `backend_factory.cpp`
  - Selects backend implementation.

## How it connects to game code
- Game uses physics objects for player and world interactions.
- Backend details are hidden from game logic.

## Common tasks
- Physics bugs: check backend implementations and the shared wrapper logic.
- Add new physics features: update both wrapper and backend.
