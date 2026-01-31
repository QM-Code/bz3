# src/engine/graphics/backends/bgfx/AGENTS.md

Read `src/engine/graphics/AGENTS.md` first.
This directory contains the bgfx graphics backend.

Key responsibilities:
- Device initialization and shutdown
- Shader/pipeline setup
- Mesh/material/texture uploads
- Render targets and frame submission
- UI render bridge integration

If a render bug only happens on bgfx, this is where to debug.
