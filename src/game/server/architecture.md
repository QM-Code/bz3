# src/game/server/architecture.md

Server flow:
- `main.cpp` starts server and initializes engine subsystems.
- `ServerWorldSession` manages world state and physics.
- Network protocol sends authoritative updates to clients.
- Plugins hook into server events for customization.
