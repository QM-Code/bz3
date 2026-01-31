# src/game/ui/controllers/AGENTS.md

Read `src/game/ui/AGENTS.md` first.
This directory contains **UI controllers** that read/write config and update
UI models.

## Controllers
- `bindings_controller.*` — load/save/reset keybindings.
- `settings_controller.*` — read/write settings (language, render scale, etc.).
- `hud_controller.*` — HUD-specific logic (fps smoothing, hints).
- `console_controller.*` — console refresh and selection logic.

## How it connects
- Controllers populate models in `ui/models/`.
- Frontends render models without duplicating business logic.
- ConfigStore is the single source of truth for settings and bindings.

## Gotchas
- Use ConfigStore APIs only; do not write JSON directly.
- Keep controller logic frontend-agnostic.
