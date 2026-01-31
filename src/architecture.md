# src/architecture.md

This document is the **top-level architecture summary** for the `src/` tree.
It’s intentionally short and conceptual; detailed subsystem architecture lives
in each subdirectory’s `architecture.md`.

## Architecture overview

### Two-layer model
- **Engine layer (Karma)**: platform + rendering + physics + input mapping + audio
  + world loading + UI bridge + ECS scaffolding.
- **Game layer (BZ3)**: gameplay simulation, network protocol, UI/HUD/console,
  world sessions, radar, and game-specific rendering logic.

### Why this structure
- Keeps core tech reusable.
- Makes the eventual engine split into a separate repo straightforward.
- Allows quick iteration while the engine surface is still evolving.

### Integration points (high level)
- **Rendering**: BZ3 uses engine renderer core and graphics backends; game renderer
  orchestrates scene + radar + UI overlay.
- **Input**: engine maps raw input into named actions; game defines action IDs and
  uses them for gameplay and roaming camera.
- **Physics**: engine owns backend and simulation; game uses physics types for
  player/world interaction.
- **UI**: game UI frontends consume engine render bridges and config store.
- **Networking**: engine provides transports; game defines protocol and sessions.

## Next steps (where to read)
- `src/engine/architecture.md`
- `src/game/architecture.md`
