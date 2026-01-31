# src/engine/audio/AGENTS.md

Read `src/AGENTS.md` and `src/engine/AGENTS.md` first.
This directory implements the engine’s **audio system** and backend factory.

## Overview
Audio is an engine-level service. Game code requests clips or plays sounds; the
engine selects the backend at build time.

Backends are selected via CMake (`KARMA_AUDIO_BACKEND`):
- `miniaudio` or `sdlaudio`

## Key files
- `audio.hpp` / `audio.cpp`
  - High-level audio API and clip cache.
  - Keeps backend-agnostic handle types.

- `backend_factory.cpp`
  - Chooses the backend implementation based on compile definitions.

- `backends/`
  - Backend-specific implementations (miniaudio, SDL3 audio).

## How it connects to game code
- Game creates an `Audio` instance through engine initialization.
- Game calls simple methods (play, load, etc.).
- Backend details are invisible to game code.

## Common tasks
- Add new backend: implement under `backends/` and update factory + CMake.
- Extend audio API: add methods in `audio.hpp` and backend interface.

## Gotchas
- Keep filenames and asset resolution in engine-level data path handling.
- Avoid BZ3-specific audio logic here.
