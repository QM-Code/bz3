# RmlUi Panels

Read `src/AGENTS.md`, `src/game/AGENTS.md`, and `src/game/ui/frontends/rmlui/AGENTS.md` first.

This directory contains RmlUi **console panels** (`panel_*.cpp`). Each panel is
responsible for a specific tab (Community, Settings, Bindings, Start Server, Docs).

Key ideas:
- Panels are instantiated by the RmlUi backend and wired into the console view.
- Each panel uses shared controllers and models where possible.
- RML/RCSS templates live in `data/client/ui/` and define layout/styles.

Notes:
- Scrollbar recipe and notes: `src/game/ui/frontends/rmlui/console/panels/docs/SCROLLBARS.md`
