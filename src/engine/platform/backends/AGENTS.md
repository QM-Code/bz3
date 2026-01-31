# src/engine/platform/backends/AGENTS.md

Read `src/engine/platform/AGENTS.md` first.
This directory contains **windowing backends**.

## Backends
- `window_sdl.*` — SDL3 implementation.
- `window_sdl2_stub.*` — stub used when SDL2 is selected (limited/no-op).

## Selection
Backend is selected at build time via `KARMA_WINDOW_BACKEND`.
