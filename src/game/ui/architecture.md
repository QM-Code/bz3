# UI Architecture

This folder contains the shared UI system plus two frontend implementations (ImGui and RmlUi).

## High-level flow
- `UiSystem` owns the selected UI backend and forwards engine events and data.
- Each backend renders to a texture (RTT) and exposes `ui::RenderOutput`.
- The renderer composites the UI texture as an overlay each frame.

## Key components
- `system.*`: lifecycle, event handling, and output retrieval.
- `backend.*`: backend interface + factory.
- `types.hpp`: shared UI types (scoreboard, render output).
- `config.*`: config helpers for required values (no defaults).
- `render_scale.*`: UI render scale (config: `ui.RenderScale`).
- `render_settings.*`: user-facing render settings (brightness, etc.).

## Bridges
- `ui/bridges/render_bridge.hpp`: renderer-agnostic bridge (radar texture).
- `ui/bridges/imgui_render_bridge.hpp`: ImGui-specific render-target bridge.
- `engine/graphics/ui_render_target_bridge.hpp`: graphics-layer RTT bridge used by ImGui.
Note: RmlUi does not use a UI-layer render bridge because it integrates directly via the RmlUi render interface inside `RmlUiBackend`.

## Frontends
- ImGui: `frontends/imgui/*`
- RmlUi: `frontends/rmlui/*`

## Feature map (entry points)
- UI render output/visibility: `frontends/*/backend.cpp`
- HUD composition: `frontends/*/hud/*`
- Console: `frontends/*/console/*`
- Start server panel: `frontends/*/console/panels/panel_start_server.*`
- Renderer integration: `frontends/*/platform/renderer_*.{hpp,cpp}`

## UI rendering policy
- Both ImGui and RmlUi render into offscreen textures.
- The render overlay uses `ui::RenderOutput` (texture + visibility).
