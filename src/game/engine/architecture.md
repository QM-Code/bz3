# src/game/engine/architecture.md

`ClientEngine` builds and wires engine subsystems, then exposes a game-friendly
API to the client layer. It owns UI/renderer wiring, brightness, language changes,
and roaming camera integration.
