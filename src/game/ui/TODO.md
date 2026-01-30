# UI Refactor Plan

## Overview

The UI subsystem should converge on a clean separation of renderer-agnostic models/actions and thin rendering backends. All UI state (HUD, Console, Settings, Bindings) should live in shared models; frontends (ImGui/RmlUi) should only render those models and emit actions. All configuration access should go through a typed facade backed by ConfigStore (no direct JSON file access). Input mapping, font loading, render output, and visibility rules should be unified so both frontends behave identically. The goal is a stable, understandable scaffolding that makes it easy to add features in one place without frontend drift.

## Full Game Plan

### Phase 0 - Target Architecture (define, then build)
- Define a small UI architecture doc (data flow and naming conventions).
- Decide on the model/action boundaries and backend contracts.
- Decide on a single rule for HUD visibility vs console presence.

### Phase 1 - Shared Input Mapping
- Create `src/game/ui/input_mapping.*` to translate `platform::Event` into UI-neutral input.
- Implement adapters for ImGui and RmlUi input ingestion.
- Normalize modifier handling (mods should be computed once and consistent across frontends).

### Phase 2 - Unified Config Access
- Introduce `ui/ui_config.*` (typed getters/setters) backed by ConfigStore.
- Remove JSON knowledge from UI state classes (RenderSettings, HudSettings).
- Ensure all UI reads/writes go through `UiConfig` (no direct JSON file IO).

### Phase 3 - Renderer-Agnostic Models
- Create `ui/models/hud_model.*` (scoreboard, chat, dialog, fps, crosshair, radar).
- Create `ui/models/console_model.*` (community list, selections, status, inputs).
- Create `ui/models/settings_model.*` and `ui/models/bindings_model.*`.

### Phase 4 - Controllers / Actions
- `HudController`: chat buffer, spawn hint, fps smoothing.
- `ConsoleController`: refresh, selection, credentials.
- `SettingsController` and `BindingsController`: persistence and validation.
- Frontends call controllers instead of mutating state directly.

### Phase 5 - Standardized Panel Framework
- One shared tab spec (order, labels, right-aligned '?').
- Shared panel lifecycle: `onShow`, `onHide`, `onTick`, `onConfigChanged`.
- Common status banner helper and shared formatting utilities.

### Phase 6 - Unified Render Output
- Implement a shared `UiRenderBridge` for both ImGui and RmlUi.
- Add render-to-texture support to RmlUi backends (BGFX/Diligent/Forge).
- Standardize output visibility: expose a definitive `RenderOutput` in both frontends.

### Phase 7 - Shared Font Loading
- Extract font config + fallback logic into a shared loader.
- ImGui consumes it for atlas build; RmlUi uses it for `LoadFontFace`.

### Phase 8 - Remove Duplication / Dead Code
- Remove duplicate NullConsole implementations (keep one).
- Consolidate `readFileBytes` and texture ID encode/decode helpers.
- Delete unused panels and old theme code (already started).

### Phase 9 - Feature Parity Decisions
- Decide final HUD features for both frontends (crosshair, dialog, spawn hint).
- Align Settings and Bindings panel behavior to avoid drift.

### Phase 10 - Verification Harness
- Add a lightweight “UI smoke” mode to render sample models without game state.
- Document expected behaviors and edge cases.

## Recommended Next Steps
1. Build `UiConfig` facade + remove JSON from `RenderSettings`/`HudSettings`.
2. Introduce `ConsoleModel` + `ConsoleController`, refactor Settings panel first.
3. Start `input_mapping.*` and hook it into ImGui backend, then RmlUi backend.

## Repo Hygiene Notes
- Forge shader outputs + `forge_data/*` logs are currently in-tree for ongoing Forge work; leave them
  for now and clean up once the Forge workstream is done.

## Backlog
- Polish HUD chat buffer limits/cleanup: cap stored chat lines, trim oldest on overflow, and
  decide whether limits should be per-session or config-driven. Ensure ImGui and RmlUi both
  rebuild from the shared model without leaking focus or breaking scroll behavior.

## Phase 5 follow-ups (detail)
- Shared formatting utilities:
  - Extract common console spacing/layout helpers (e.g., banner spacing, section headers,
    label/value alignment) so both ImGui and RmlUi panels present consistent rhythm.
  - Normalize status banner presentation (prefix/icon, padding, and error/pending tones) across
    both frontends, including any future status lines in Community/Start Server panels.
- Tab order spec refinements:
  - Centralize tab order + labels in the shared spec, then ensure both frontends use it for
    ordering, right-aligned placement, and refresh-on-activate behavior.
  - Extend spec to include flags for conditional visibility (e.g., hide Start Server when
    server binary missing) and align the fallback “panel missing” message in RmlUi.
- Right-aligned “?” consistency:
  - Ensure the documentation tab is right-aligned in both ImGui and RmlUi.
  - Add a shared rule for minimum spacing from the tab row edge so the “?” tab doesn’t shift
    when localized labels change widths.
