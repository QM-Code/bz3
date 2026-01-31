# src/engine/audio/backends/AGENTS.md

Read `src/engine/audio/AGENTS.md` first.
This directory contains **backend-specific** audio implementations.

## Backends
- `miniaudio/` — lightweight cross-platform audio.
- `sdl/` — SDL3 audio backend.

## How selection works
`KARMA_AUDIO_BACKEND` controls which backend is compiled.
`backend_factory.cpp` instantiates the selected backend.

## Adding or changing a backend
- Implement a concrete backend under this directory.
- Wire it in `backend_factory.cpp`.
- Update `src/engine/CMakeLists.txt` to include sources.
