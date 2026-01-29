# UI AGENTS.md

This file is UI-specific onboarding for future coding agents working under `src/game/ui/`.
It complements the repo-level AGENTS.md and focuses on how the UI subsystem is organized,
what the important files do, and where the current refactors are headed.

## Quick map (what lives where)

### Core UI entry points
- `src/game/ui/system.*`:
  - Owns the high-level UI system.
  - Bridges config changes into HUD visibility (via `ConfigStore`).
  - Calls `backend->update()` each frame.
  - Key logic: HUD visible when connected OR console is hidden (see `UiSystem::update`).

- `src/game/ui/backend.hpp`:
  - Interface for UI backends.
  - Both ImGui and RmlUi implement this.
  - Exposes `console()`, `setHudModel()`, `getRenderOutput()`, etc.

- `src/game/ui/backend_factory.cpp`:
  - Picks ImGui or RmlUi backend at build time.

### Render settings and HUD settings
- `src/game/ui/render_settings.*`:
  - Brightness state and dirty tracking.
  - Currently used by both frontends.
- `src/game/ui/hud_settings.*`:
  - HUD visibility toggles (scoreboard/chat/radar/fps/crosshair).

### Frontend backends (ImGui + RmlUi)
- ImGui: `src/game/ui/frontends/imgui/`
  - `backend.*`: ImGui frame lifecycle, input processing, render-to-texture via ImGui render bridge.
  - `console/`: UI console panels and logic (community/settings/bindings/start server/docs).
  - `hud/`: HUD rendering (chat/radar/scoreboard/crosshair/fps).

- RmlUi: `src/game/ui/frontends/rmlui/`
  - `backend.*`: RmlUi context + doc loading + render loop.
  - `console/`: panels are class-based (`panel_*`), RML/RCSS templates live in `data/client/ui`.
  - `hud/`: RmlUi HUD document and components.

### Rendering bridges (BGFX/Diligent/Forge)
- ImGui uses `engine/graphics/ui_bridge.hpp` and backend-specific bridges under
  `src/engine/graphics/backends/*`.
- RmlUi uses render interfaces in `src/game/ui/frontends/rmlui/platform/renderer_{bgfx,diligent,forge}.*`.
- RmlUi renderers currently do NOT expose output textures in a unified way; this is a known refactor target
  (see `src/game/ui/TODO.md`).

## Console vs HUD

- The **Console** is the in-game UI with tabs (Community, Start Server, Settings, Bindings, ?).
- The **HUD** is gameplay overlay (chat, radar, scoreboard, crosshair, fps, dialog).
- The HUD draws beneath the console when connected; when not connected and the console is open,
  the HUD is hidden. This is enforced in `UiSystem::update` and backend draw logic.
- Crosshair is explicitly suppressed when the console is visible to avoid the “white square” leak.

## ConfigStore usage (important)

- All UI config reads/writes must go through `ConfigStore` (no direct JSON file I/O).
- The UI should only use:
  - `bz::config::ConfigStore::Get/Set/Erase` for config values.
  - `bz::config::ConfigStore::Revision()` to detect changes.
- Example usage is in:
  - RmlUi Settings panel (`panel_settings.cpp`)
  - ImGui Settings panel (`panel_settings.cpp`)
  - Console community credentials in both frontends.

## Notable recent changes (current state)

- Tabs and layout:
  - Both frontends now use the order: Community -> Start Server -> Settings -> Bindings -> ?.
  - The "?" tab is right-aligned in both (RmlUi via CSS flex; ImGui via cursor positioning).
- Documentation panel now shows: "This space intentionally left blank."
- Themes were removed (ImGui theme panel deleted; RmlUi theme panel removed).
- HUD visibility is controlled by connection state and console visibility.
- Brightness is commit-on-release in both frontends; brightness pass disabled while dragging.
- Keybindings were extracted to `src/game/ui/console/keybindings.*` for shared helpers.

## Files worth reading first

1) `src/game/ui/backend.hpp` and `src/game/ui/system.cpp` (core API + update rules)
2) `src/game/ui/frontends/imgui/backend.cpp` (ImGui render loop + HUD/console draw order)
3) `src/game/ui/frontends/rmlui/backend.cpp` (RmlUi context + HUD/console docs)
4) `src/game/ui/frontends/imgui/console/console.*` (ImGui console tabs, panels, state)
5) `src/game/ui/frontends/rmlui/console/console.*` (RmlUi console + panel wiring)
6) `src/game/ui/frontends/rmlui/console/panels/*` (panel behavior + ConfigStore usage)
7) `src/game/ui/TODO.md` (current refactor plan)

## Frontend-specific notes

### ImGui
- Rendering order: HUD then Console. Console visibility affects crosshair drawing.
- `console/console.cpp` owns tab layout; `panel_*` files implement UI.
- Community refresh should trigger on tab activation or click.

### RmlUi
- Document load order: HUD document is loaded before console to ensure HUD renders beneath.
- Panels are instantiated in `backend.cpp` and registered to console view.
- RML layout lives in `data/client/ui/*.rml` and styles in `*.rcss`.

## Known TODOs / planned refactor (summary)

- Create a shared input mapping layer (`input_mapping.*`) to remove duplicated key/mod handling.
- Create a ConfigStore facade (`ui_config.*`) to centralize typed config access.
- Move UI state into renderer-agnostic models + controllers (HUD/Console/Settings/Bindings).
- Unify render-to-texture output between ImGui and RmlUi.
- Consolidate font loading logic and fallback behavior.
- Remove remaining utility duplication (file reads, texture id conversions, null console).

## Gotchas

- Console visibility and HUD visibility are deliberately coupled in `UiSystem::update`.
- ConfigStore has save/merge intervals and revision tracking; use `Revision()` to resync UI state.
- Start Server panels still write a JSON override file for server instances (left as-is for now).

