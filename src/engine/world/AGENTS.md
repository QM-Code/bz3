# src/engine/world/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory implements **engine-level world content loading**.

## Responsibilities
- Resolve and load world content from backends (filesystem today).
- Provide content lookup to the game layer.

## Key files
- `content.*` — content registry and resolution helpers.
- `backend_factory.cpp` — selects the world backend.
- `backends/fs/` — filesystem backend.

## How it connects to game code
Game world/session logic requests assets via engine content APIs; the engine
handles resolution and loading.
