# GUI/HUD

Use this prompt when working on the MainMenu and its panels.

## Start here
- Main wrapper: `src/engine/components/gui/main_menu.hpp` and `src/engine/components/gui/main_menu.cpp`.
- Panels live in `src/engine/components/gui/panels/`:
  - `community_browser_panel.cpp`
  - `start_server_panel.cpp`
  - `themes_panel.cpp`
  - `settings_panel.cpp`, `documentation_panel.cpp` (placeholders)
- The MainMenu wrapper renders the top header and tabs, then delegates each panel’s draw.

## Key flow
- `GUI::update()` calls `mainMenuView.draw(...)` and handles font reload requests.
- MainMenu orchestrates state shared across panels (e.g., list options, auth inputs, log output).
- Community panel handles server list selection, join flow, and user identity input.
- Start Server panel manages local `bz3-server` processes and log streaming.
- Themes panel persists user theme overrides to the user config.

## Integration points
- Community panel and Start Server panel share the community list (`listOptions`) so the server launcher can pick a community to send heartbeats to.
- Community auth flow is in `src/client/server/community_browser_controller.cpp` and uses the UI state from `MainMenuView`.
- Thumbnail loading is in `src/engine/components/gui/thumbnail_cache.hpp` and `src/engine/components/gui/thumbnail_cache.cpp`.
- Server heartbeat override uses `-C/--community` (see `src/server/community_heartbeat.cpp`).

## Guardrails
- Do not store plaintext passwords. Password hashes are used for auth when needed.
- LAN mode should never use passwords.
- Keep UI responsive; background work stays off the UI thread.
