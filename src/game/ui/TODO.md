# UI Roadmap (current)

This file describes *only* what remains and where to start. Completed work is
tracked in git history and `architecture.md`.

## Phase A - Radar + Render Target Stability (urgent)
Goal: fix intermittent radar dots/player blip disappearing across all backends.

Tasks:
- Add targeted logging around radar render target creation + texture id lifetime
  in `src/game/renderer/render.cpp` and graphics backends.
- Verify radar render target survives across frames and is not invalidated by
  resize/reset or backend init order.
- If needed, re-create radar render target when backend swaps or textures are lost.

Exit criteria:
- Shots + player blip remain stable on radar while standing still.
- Verified on BGFX + Diligent (ImGui + RmlUi).

## Phase B - Feature Parity Decisions
Goal: lock behavior so ImGui and RmlUi remain in sync.

Tasks:
- Decide final HUD feature parity (crosshair, dialog, spawn hint).
- Align Settings + Bindings panel behaviors to match 1:1.

Exit criteria:
- One documented decision set; both frontends match.

## Phase C - Verification Harness
Goal: quick UI regression testing without a running game.

Tasks:
- Add a lightweight UI “smoke mode” to render sample models.
- Document expected behaviors + edge cases.

Exit criteria:
- One simple command/path to validate UI visuals + input.

## Phase D - Backlog (low priority)
- HUD chat buffer limits: cap stored lines, trim oldest on overflow, decide
  session vs config-driven limit. Ensure both frontends remain in sync.

## Repo Hygiene Notes
- Forge shader outputs + `forge_data/*` logs are in-tree for ongoing Forge work; leave them
  for now and clean up once the Forge workstream is done.
