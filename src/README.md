# src/README.md

This README provides a **high-level** map of `src/`. For deeper technical detail,
read the cascading `AGENTS.md` and `architecture.md` files.

## Big picture
The project is intentionally split into two domains:

- **Karma engine** (`src/engine/`): reusable game-agnostic systems.
- **BZ3 game** (`src/game/`): BZ3-specific gameplay, UI, and net protocol.

These live together during early development for fast iteration. Long-term, Karma
will be a standalone repo with its own build and release cadence, and BZ3 will
consume Karma as an external dependency.

## Important: how to work here
You should **not** dive into implementation details or edit code directly unless
you already know this codebase very well. The recommended workflow is to describe
what you want in high-level terms and let an agent map that intent to the right
files and changes.

Examples:
- “Please add a new keybinding action for roaming mode and expose it in the UI.”
- “The HUD radar needs a new overlay element; update both ImGui and RmlUi.”

## How to talk to an agent
Examples:

- “Please get familiar with the engine renderer. Start at
  `src/engine/README.md`, then read `src/engine/renderer/README.md`.”
- “We’re working on game UI. Read `src/game/README.md` then `src/game/ui/README.md`.”

## Where to go next
- If your task is **engine-related**, open `src/engine/README.md`.
- If your task is **gameplay/UI/net**, open `src/game/README.md`.
