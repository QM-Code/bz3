# Engine Refactor Plan (Engine‑Purist, Karma‑Style)

## Context for Handoff (Read First)

We are **preparing** (not executing yet) a major refactor to align BZ3 with an
engine‑purist architecture that matches the usage model demonstrated in:

`/home/karmak/dev/karma/examples/main.cpp`

The **reference engine** lives in:

`/home/karmak/dev/karma/`

Please study that repo first, especially:
- `README.md` (engine skeleton + layout)
- `docs/ENGINE_IMPLEMENTATION.md` (engine ownership + systems)
- `docs/ENGINE_USAGE.md` (GameInterface, EngineApp usage)
- `examples/main.cpp` (intended game dev workflow)
- `include/karma/*` (public header surface we want to emulate)

### What the end state should look like

We want **BZ3** to consume **Karma** as if it were a standalone library:

- `src/engine/` should become the **engine library implementation**.
- Public headers should live under `include/karma/*` (like the Karma repo).
- `src/game/` should **only** include `karma/...` headers and link against the
  engine library; it should not compile engine sources directly.

In other words, the game should look like a consumer of an external engine
library, and the engine should own the main loop and systems. The gameplay code
should only manipulate ECS data and respond to input, not orchestrate rendering
or subsystems directly.

### Why we are doing this

The current BZ3 structure mixes “game‑owned orchestration” with “engine‑provided
subsystems.” The desired model is the **engine‑purist** approach shown in the
Karma example: the engine owns lifecycle, tick, and systems; the game provides
data/logic via ECS and an Overlay interface.

### Key architectural expectations

1) **Engine owns the loop and systems.** Game code should not drive render
   passes, input pump, or physics steps directly.
2) **ECS is the source of truth.** Game creates entities + components; systems
   render/simulate based on ECS data. Avoid game‑level render IDs.
3) **UI is an overlay.** Game supplies an Overlay implementation; engine renders
   via backend(s).
4) **Public API = include/karma/**. Anything not intended for external use must
   remain internal to `src/engine/`.

### Practical constraints

We are **not executing this refactor yet**; this document is the plan. If you
start the work, do it in phases, keep the build functioning where possible, and
expect broad changes across `src/game/*`, `src/engine/*`, CMake, and docs.

This plan aligns `src/engine/` with the Karma example usage model and makes
`src/game/` consume the engine strictly as a library + headers (no direct
engine source compilation). The default assumption is an **engine‑purist**
approach where the engine owns the main loop and systems, and the game supplies
ECS data + gameplay logic.

## Goals

1) `src/engine/` becomes a standalone **Karma engine** (library + headers).
2) `src/game/` uses only the public engine API (no engine source compilation).
3) Game dev flow matches `/home/karmak/dev/karma/examples/main.cpp`.

## Phase 0 — Baseline & Coupling Audit

**Goal:** fully understand current engine/game coupling and public API usage.

Tasks:
- Inventory every `karma/...` include in `src/game/`.
- Identify direct game usage of engine subsystems (render, input, audio,
  physics, network, config, data paths).
- Identify orchestration that currently lives in game (render passes, UI/HUD,
  input gating, world session control).
- Identify which subsystems already behave like engine systems and which are
  game‑owned.

Deliverable:
- Coupling map: `src/game/` → engine subsystems, including source‑level
  dependencies.
- List of orchestration responsibilities that must move into engine.

## Phase 1 — Define the Public Engine API (Karma‑Style)

**Goal:** lock a stable engine API that matches the Karma example.

Tasks:
- Public classes/headers:
  - `GameInterface` (`onStart/onUpdate/onFixedUpdate/onShutdown`).
  - `EngineApp` (`start/tick/isRunning`).
  - `EngineConfig` (window, renderer, physics, audio, network, config/data).
  - ECS types: `World`, `Entity`, component registry.
  - Input API: bind actions, query action states.
  - Overlay API: `Overlay` interface for ImGui/RmlUi.
- Decide what is **public** vs **internal**. Public API should not expose
  renderer internals or GPU handles.

Deliverable:
- A public header set that mirrors the Karma example usage.

### Target public header surface (mirrors `/home/karmak/dev/karma/include/karma`)

Intended end state for public headers:

- `karma/app/*` — EngineApp, GameInterface, EngineConfig
- `karma/core/*` — core types, IDs, math helpers
- `karma/ecs/*` — World, Entity, component registry
- `karma/components/*` — shared gameplay‑ready components
- `karma/renderer/*` — renderer interfaces + resource descriptors (public only)
- `karma/graphics/*` — minimal graphics types (public only)
- `karma/scene/*` — scene graph (if retained)
- `karma/systems/*` — system interfaces & scheduling (if exposed)
- `karma/input/*` — action binding + input query API
- `karma/audio/*` — audio public API (no backend details)
- `karma/physics/*` — physics public API (no backend details)
- `karma/network/*` — transport API (public only)
- `karma/ui/*` — Overlay interface + UI types

Anything not intended for external use should live only under `src/engine/`
and not be reachable from `include/karma/*`.

## Phase 2 — Engine Owns the Loop & Systems

**Goal:** engine is responsible for lifecycle, timing, and systems.

Tasks:
- Move/centralize main loop, timing, and fixed‑step scheduling into
  `src/engine/app`.
- Engine owns input pump and config initialization.
- Engine systems operate on ECS data:
  - Renderer uses `Transform + Mesh + Material + Light + Camera`.
  - Physics uses `Transform + Collider + Rigidbody`.
  - Audio uses `Transform + AudioSource + Listener`.
- Remove any game‑side direct orchestration of these systems.

Deliverable:
- Engine can run a minimal ECS‑only example without game‑side render calls.

## Phase 3 — Remove Game‑Owned Rendering

**Goal:** eliminate `src/game/renderer/` and move render orchestration into
engine or engine‑provided extension points.

Tasks:
- Migrate BZ3 render orchestration into engine renderer system (or plug‑in
  extension API).
- Replace render IDs / renderer handles with ECS components.
- Move HUD/console rendering into Overlay implementations.
- Game supplies data; engine draws.

Deliverable:
- `src/game/renderer/` removed; render path is engine‑owned.

## Phase 4 — Build as a Real Engine Library

**Goal:** `src/engine/` builds as a standalone library, with installed headers.

Tasks:
- Create `karma` library target (static/shared).
- Install/export headers under `karma/...`.
- Remove in‑tree forwarders `src/engine/karma/*`.
- Update game targets to link against `libkarma`.

Deliverable:
- `src/game/` compiles without direct source access to engine internals.

## Phase 5 — Refactor Game to GameInterface Model

**Goal:** BZ3 code looks like `karma/examples/main.cpp`.

Tasks:
- Replace game‑owned loop with `EngineApp::start()` + `tick()`.
- Game logic only manipulates ECS components / input actions.
- Replace direct engine calls with component changes.
- Move config/data bootstrap into `EngineConfig`.

Deliverable:
- Game code is mostly gameplay logic + ECS data.

## Phase 6 — Validation & Docs

**Goal:** new architecture is documented and reliable.

Tasks:
- Update README / architecture / AGENTS files with new structure.
- Add a minimal example in this repo that mirrors the Karma example.
- Confirm engine can be extracted into its own repo cleanly.

Deliverable:
- Clear docs + example + usable engine library boundary.

---

### Notes / Assumptions
- This plan assumes an engine‑purist architecture where the engine owns the
  main loop, render system, and timing. The game provides data and gameplay
  logic via ECS/components and overlays.
- If the goal shifts toward a hybrid model, Phase 3 and Phase 5 would be
  reduced or replaced with explicit render extension points.
