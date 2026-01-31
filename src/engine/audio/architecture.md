# src/engine/audio/architecture.md

## Structure
- `audio.*` defines the public engine audio API.
- `backend_factory.cpp` selects a backend at build time.
- `backends/` contains concrete implementations.

## Integration
The engine initializes audio early and hands a single `Audio` object to the game.
The game never talks to backend-specific classes.
