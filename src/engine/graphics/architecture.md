# src/engine/graphics/architecture.md

## Layers
1) **Backend factory** — compile-time selection (bgfx/diligent).
2) **Device wrapper** — backend-agnostic surface for renderer.
3) **Resource registry** — caches meshes/materials/textures and handles refcounts.
4) **Backend implementations** — actual GPU API usage.

## Data flow
Renderer → Graphics device → Backend → GPU

UI renderers also depend on backend-specific bridges to create texture targets
for ImGui/RmlUi.
