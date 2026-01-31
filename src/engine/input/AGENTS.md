# src/engine/input/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory implements **action-agnostic input mapping**.

## Purpose
- The engine maps platform events to **named actions**.
- The game defines the action IDs and uses them (e.g., `moveForward`).

## Key files
- `input.*`
  - The engine input object. Holds an `InputMapper` and a default map.
  - Loads bindings from ConfigStore (`keybindings`).

- `bindings_text.*`
  - Converts bindings to/from display strings.

- `mapping/`
  - Binding parsing, maps, and mapper logic.

## How it connects to the game
- Game defines the action strings (see `src/game/input/actions.hpp`).
- Game passes default bindings into engine input during initialization.
- UI shows/edit bindings via the same action IDs.

## Gotchas
- Input is **action-based**, not key-based.
- Config is required; missing `keybindings` throws a fatal error.
