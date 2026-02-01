# src/engine/ui/platform/imgui/AGENTS.md

Read `src/engine/ui/platform/AGENTS.md` first.
This directory contains imgui-specific render interface helpers.

Typically this is where backend-specific hooks for imgui live (bgfx/diligent).

## Rendering gotchas
- ImGui expects straight alpha blending. The bgfx renderer should use
  `SRC_ALPHA / INV_SRC_ALPHA` (not premultiplied) or fonts/borders may render
  incorrectly.
