# src/game/ui/frontends/AGENTS.md

Read `src/game/ui/AGENTS.md` first.
This directory contains the **two UI frontend implementations**:

- `imgui/` — immediate-mode UI implementation
- `rmlui/` — RmlUi (HTML/CSS-like) implementation

Both frontends implement the same backend interface and consume shared models
from `src/game/ui/models/`.

## How to choose where to work
- If you need visual/layout changes: edit the frontend-specific code.
- If you need logic changes: check controllers and models first.

## Parity policy
Features should stay in sync across both frontends. When adding a panel or
interaction, implement it in both ImGui and RmlUi.
