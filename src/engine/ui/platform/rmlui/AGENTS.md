# src/engine/ui/platform/rmlui/AGENTS.md

Read `src/engine/ui/platform/AGENTS.md` first.
This directory contains rmlui-specific render interface helpers.

Typically this is where backend-specific hooks for rmlui live (bgfx/diligent).

## Alpha blending (bgfx)
- RmlUi supplies premultiplied vertex colors (`Rml::ColourbPremultiplied`).
- The bgfx renderer must use premultiplied blending:
  `ONE, INV_SRC_ALPHA` (see `renderer_bgfx.cpp`).
- Do not switch this to straight-alpha blending, or HUD opacity will look too faint.
