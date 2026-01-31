# src/game/ui/console/AGENTS.md

Read `src/game/ui/AGENTS.md` first.
This directory defines **console abstractions** shared by UI backends.

## Key files
- `console_interface.hpp`
  - Abstract interface used by the game to interact with console UI.

- `tab_spec.*`
  - Defines console tab ordering and labels.

- `keybindings.*`
  - Lists input action definitions for the bindings panel.

## How it connects
Both ImGui and RmlUi console views implement `ConsoleInterface`.
