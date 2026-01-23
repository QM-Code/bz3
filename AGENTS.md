# AGENTS.md

This file provides quick, repo-specific instructions for coding agents.

## Project summary
- BZ3 is a C++20 client/server 3D game inspired by BZFlag.
- Two binaries: `bz3` (client) and `bz3-server` (server).
- Runtime assets/config resolve from `BZ3_DATA_DIR` (usually `data/`).

## Key directories
- `src/client/`: client gameplay and main loop.
- `src/server/`: server gameplay and main loop.
- `src/engine/`: shared systems (render, physics, network, input, audio).
- `src/ui/`: UI entry point, backends, and shared UI interfaces.
- `src/common/`: config/data root resolution helpers.
- `src/protos/`: protobuf schema.
- `data/`: configs, assets, worlds, plugins.
- `webserver/`: optional Python community server.

## Build and run
Linux/macOS:
- `./setup.sh`
- `cmake --build build`
- `BZ3_DATA_DIR="$PWD/data" ./build/bz3`
- `BZ3_DATA_DIR="$PWD/data" ./build/bz3-server`

Windows:
- `setup.bat`
- `cmake --build build --config Release`
- `set BZ3_DATA_DIR=%CD%\data`
- `build\Release\bz3.exe`
- `build\Release\bz3-server.exe`

## Common workflows
- New networked feature: update `src/protos/messages.proto`, then encode/decode in
  `src/engine/components/*_network.*`, then handle in `src/client/*` and/or `src/server/*`.
- World loading changes: `src/server/world.*` and `src/client/world.*`.
- UI/HUD changes: `src/ui.*`.
- Plugins: `src/server/plugin.*` and `data/plugins/*`.

## Notes / gotchas
- Config is layered via `src/common/data_path_resolver.*`; prefer asset keys over hard paths.
- Network messages are "peeked" and must be freed on `flushPeekedMessages()`. Do not store pointers.

## Prompt index
Prompt files live in `docs/agent-prompts/`. Ask to use one by name (title) or file.

- Webserver: `docs/agent-prompts/webserver.md`
- UiSystem/HUD: `docs/agent-prompts/gui-hud.md`
- Template: `docs/agent-prompts/_TEMPLATE.md`

## Tests
- None automated in this repo (manual run is typical).
- If `python` is missing, prefer `python3` for scripts/tools.
