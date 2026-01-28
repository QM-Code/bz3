# AGENTS.md

This file provides quick, repo-specific instructions for coding agents.

## Project summary
- BZ3 is a C++20 client/server 3D game inspired by BZFlag.
- Two binaries: `bz3` (client) and `bz3-server` (server).
- Runtime assets/config resolve from `BZ3_DATA_DIR` (usually `data/`).

## Key directories
- `src/game/client/`: client gameplay and main loop.
- `src/game/server/`: server gameplay and main loop.
- `src/game/engine/`: BZ3-specific engine orchestrators.
- `src/engine/core/`: shared types and constants.
- `src/engine/graphics/`: engine-agnostic graphics API + backends.
- `src/game/renderer/`: BZ3 render orchestration and particles.
- `src/engine/geometry/`: mesh loading utilities.
- `src/engine/audio/`: audio system.
- `src/engine/input/`: input handling.
- `src/engine/physics/`: physics world and bodies.
- `src/engine/network/`: networking transports.
- `src/game/net/`: game protocol, codec, and message-level networking.
- `src/engine/platform/`: platform glue (GLFW callbacks).
- `src/game/ui/`: UI entry point, backends, and shared UI interfaces.
- `src/engine/common/`: config/data root resolution helpers.
- `src/game/protos/`: protobuf schema.
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
- New networked feature: update `src/game/protos/messages.proto`, then encode/decode in
  `src/game/net/*`, then handle in `src/game/client/*` and/or `src/game/server/*`.
- World loading changes: `src/game/server/world_session.*` and `src/game/client/world_session.*`.
- UI/HUD changes: `src/game/ui.*`.
- Plugins: `src/game/server/plugin.*` and `data/plugins/*`.

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

## Filament SDK (Wayland/Vulkan)
- Filament Vulkan on Wayland requires rebuilding the Filament SDK (clang + libstdc++).
- Use `scripts/build_filament.sh` to clone/build/install into `libs/filament-sdk`.
- If you need a specific Filament version, set `FILAMENT_REF` before running the script.
- Clean rebuild (recommended when changing Filament versions or toolchains):
  `CLEAN_INSTALL=1 ./scripts/build_filament.sh`
 - The build script forces `USE_STATIC_LIBCXX=OFF` to keep ABI compatibility with libstdc++.
