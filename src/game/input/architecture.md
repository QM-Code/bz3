# src/game/input/architecture.md

The game defines action IDs and default bindings. The engine input mapper
binds physical keys/mouse buttons to these actions. Gameplay queries input
through action names, not raw keys.
