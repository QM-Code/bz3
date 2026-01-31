# src/game/ui/models/AGENTS.md

Read `src/game/ui/AGENTS.md` first.
This directory contains **shared UI models** used by both frontends.

## Models
- `hud_model.*` — HUD visibility + data (chat, radar, dialog, etc.).
- `console_model.*` — Console tab and selection state.
- `settings_model.*` — Settings UI state.
- `bindings_model.*` — Keybinding UI state.

## How they’re used
Controllers populate these models, then backends render them. This keeps UI
logic frontend-agnostic.
