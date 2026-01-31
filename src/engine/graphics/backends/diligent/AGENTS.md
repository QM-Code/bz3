# src/engine/graphics/backends/diligent/AGENTS.md

Read `src/engine/graphics/AGENTS.md` first.
This directory contains the diligent graphics backend.

Key responsibilities:
- Device initialization and shutdown
- Shader/pipeline setup
- Mesh/material/texture uploads
- Render targets and frame submission
- UI render bridge integration

If a render bug only happens on diligent, this is where to debug.
