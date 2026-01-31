# src/engine/ui/platform/AGENTS.md

Read `src/engine/ui/AGENTS.md` first.
This directory contains **RmlUi render interfaces** for each graphics backend.

## Backends
- `renderer_bgfx.*`
- `renderer_diligent.*`

These implement `Rml::RenderInterface` and are responsible for:
- Creating UI render targets
- Translating RmlUi geometry into backend draw calls
- Managing texture caches

## How it connects to the game
- Game RmlUi frontend uses these render interfaces when constructing its
  RmlUi context.
- The output texture is exposed via engine UI bridges for composition.

## Gotchas
- DPI scaling and render scale adjustments must be handled here.
- Always keep parity between bgfx and diligent paths.
