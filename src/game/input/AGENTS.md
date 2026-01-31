# src/game/input/AGENTS.md

Read `src/game/AGENTS.md` first.
This directory defines **game-specific input actions** and default bindings.

## Key files
- `actions.hpp`
  - Defines action string IDs (e.g., `moveForward`, `roamLook`).

- `bindings.*`
  - Default bindings for all actions.
  - These defaults are merged with config (ConfigStore).

- `state.*`
  - High-level input state struct used by gameplay code.

## How it connects
- Engine input mapping uses these action IDs.
- UI bindings panel lists these actions and edits config.
