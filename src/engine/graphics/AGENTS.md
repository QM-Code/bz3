# src/engine/graphics/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory owns the **graphics device layer** and backend selection.

## Responsibilities
- Choose graphics backend at build time (BGFX or Diligent).
- Provide a backend-agnostic graphics device interface.
- Manage GPU resources (meshes, materials, textures, render targets).

## Key files
- `backend_factory.cpp`
  - Selects the backend based on compile definitions.

- `device.*`
  - High-level device wrapper used by renderer code.

- `resources.*`
  - Resource registry and caches (mesh/material/texture).

- `backends/`
  - Backend-specific implementations (bgfx, diligent).

## How it connects
- Renderer core requests GPU resources and submits draw calls via the backend.
- UI renderers rely on backend-specific bridges to render into textures.

## Common tasks
- Add a render path: usually in renderer, but may require device changes.
- Fix GPU resource bugs: start in resource registry and backend.
- Backend-specific bugs: go directly to `backends/{bgfx|diligent}`.

## Gotchas
- Backend selection is compile-time; avoid runtime branching in game code.
- Keep backend-agnostic interfaces minimal and stable.
