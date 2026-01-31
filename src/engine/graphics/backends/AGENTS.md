# src/engine/graphics/backends/AGENTS.md

Read `src/engine/graphics/AGENTS.md` first.
This directory contains backend-specific implementations.

## Backends
- `bgfx/` — BGFX renderer backend.
- `diligent/` — Diligent Engine backend.

## What lives here
- Backend initialization/shutdown
- Render target creation and management
- Material/shader compilation and pipelines
- Texture uploads and caches
- UI bridge integration (render-to-texture)

## Debugging guidance
- If a rendering issue is backend-specific, start here.
- Compare code paths between bgfx and diligent to find divergence.

## Adding a backend
Requires new backend classes, CMake wiring, and factory changes. This is
non-trivial and should be done in the engine repo once Karma is split.
