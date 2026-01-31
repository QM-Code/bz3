# src/engine/world/backends/AGENTS.md

Read `src/engine/world/AGENTS.md` first.
This directory contains world content backend implementations.

## Current backend
- `fs/` — loads content from the filesystem under `KARMA_DATA_DIR`.

## Selection
`KARMA_WORLD_BACKEND` chooses the backend at build time (fs only today).
