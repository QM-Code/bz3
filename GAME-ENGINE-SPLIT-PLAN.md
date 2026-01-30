# Game/Engine Split Plan

This document is a migration playbook for separating the engine from the BZ3 game so the two can live in separate repos. The goal is a reusable engine (“karma”) under `src/engine/` with stable headers (`karma/...`) and a thin game layer under `src/game/` that depends on the engine. It is written to guide a future agent step-by-step through the migration with minimal guesswork.

## Target shape (repo split ready)

- **Engine repo (karma)**: `src/engine/` only, headers exported as `karma/...`, no includes from `src/game/`, no game assets.
- **Game repo (bz3)**: `src/game/` only, depends on `karma` for runtime, ECS, graphics, input, audio, physics, networking, UI overlay plumbing.
- **Entry points**: game `main.cpp` instantiates `karma::app::EngineApp` and provides a `karma::app::GameInterface` implementation.
- **UI overlay**: engine exposes a render-to-texture overlay interface; game implements HUD/Console on top.
- **ECS**: engine provides `ecs::World` + `SystemGraph`; game registers components/systems.

## What should remain in `src/game/`

These modules are specific to the BZ3 game (tanks, shots, HUD/Console, BZ3 protocol) and should stay in the game layer.

- **Gameplay logic**: `src/game/client/*`, `src/game/server/*` (actors, shots, chat, world_session, game rules).
- **Game protocol**: `src/game/net/messages.hpp`, `src/game/net/proto_codec.*`, `src/game/protos/messages.proto`.
- **Game UI**: HUD, Console, bindings/settings panels, community browser, etc.
  - All of `src/game/ui/frontends/*`, `src/game/ui/controllers/*`, `src/game/ui/models/*`, `src/game/ui/console/*`, `src/game/ui/config/*`.
- **Game input actions/bindings**: `src/game/input/*` (actions, default bindings, state).
- **Game asset/config spec**: `src/game/common/data_path_spec.*`, `src/game/world/config.*`.

## What should move (or be split) into `src/engine/`

These modules are engine-agnostic or are toolkit glue that should be reusable by a different game.

### 1) Rendering core vs game rendering
- `src/game/renderer/render.*` mixes engine-level scene orchestration with game-specific radar/HUD logic.
- Proposed split:
  - `src/engine/renderer/` (or `src/engine/graphics/scene/`): core scene renderer, camera, render targets, entity creation, materials, UI overlay plumbing.
  - `src/game/renderer/`: game-specific rendering features (radar, FOV lines, scoreboard bindings, theme decisions).
  - Engine should expose hooks for secondary layers (radar, overlays), but gameplay defines what they represent.

### 2) UI toolkit plumbing vs game UI
- The UI frontends (ImGui/RmlUi) are game-specific because they encode HUD/Console structure.
- But render bridges and backend glue are engine-agnostic:
  - Move `src/game/ui/bridges/*` into `src/engine/ui/bridges/*`.
  - Move toolkit platform renderers (`renderer_bgfx/diligent/forge.*`) under `src/engine/ui/platform/`.
  - Keep `src/game/ui/frontends/*` for layout, panels, HUD widgets.

### 3) Networking
- Engine already has transports in `src/engine/network/*`.
- `src/game/net/backends/enet/*` duplicates that role.
- Proposed split:
  - Move transport backends to engine (or remove `src/game/net/backends/enet/*` in favor of engine transport).
  - Keep protocol + message codec in game (`messages.proto`, `proto_codec.*`).
  - `client_network/server_network` become game-protocol adapters on top of engine transport.

### 4) Input
- Engine has mapping in `src/engine/input/mapping/*`.
- Game uses `src/game/input/*` for actions/bindings/state.
- Keep action definitions in game; consider moving shared binding storage/serialization into engine.

### 5) World/content
- Engine already has generic world backends (`src/engine/world/*`).
- Game world config + gameplay constraints stay in `src/game/world/*`.
- Any purely data-loading logic (mesh/asset extraction) can move to engine.

### 6) Runtime scaffolding (karma-style EngineApp)
- Introduce `src/engine/app/engine_app.*` with `karma::app::EngineApp`.
- Define `karma::app::GameInterface` for game lifecycle hooks.
- Game provides `Bz3ClientGame` / `Bz3ServerGame` that implement GameInterface.
- `src/game/engine/client_engine.*` / `server_engine.*` become thin adapters or are replaced by the EngineApp runtime.

## Suggested staged migration plan (aligned with karma model + ECS)

### Phase 0 — Stabilize boundaries (no behavior changes)
- **Add a `karma` include root** for engine headers while keeping existing includes working.
  - Update engine headers to live under `src/engine/...` but be exported as `karma/...` (add include path mapping in CMake).
  - Replace includes in engine code to use `karma/...` (leave game code for later).
- **Block engine → game includes**:
  - Add a quick CI or local build check to detect includes from `src/game/` inside `src/engine/`.
  - For any current engine file that reaches into game types, add a temporary forward declaration or an interface in engine (to be filled in Phase 1/2).

### Phase 1 — Engine runtime shell (EngineApp)
- **Create `src/engine/app/engine_app.*`**:
  - Owns window, render device, audio, physics, input, network, time.
  - Exposes `run()` / `tick()` loop and lifecycle hooks.
- **Define `karma::app::GameInterface`**:
  - `onInit`, `onShutdown`, `onUpdate`, `onRender`, and optional hooks for UI overlay.
  - Provide accessors to engine subsystems (via a lightweight context object).
- **Wire BZ3 to EngineApp**:
  - Create `Bz3ClientGame` / `Bz3ServerGame` implementing `GameInterface`.
  - Update `src/game/client/main.cpp` and `src/game/server/main.cpp` to instantiate EngineApp and pass the game object.
  - Keep existing `ClientEngine`/`ServerEngine` for now, but call them from inside `Bz3*Game` (thin adapters).

### Phase 2 — Engine UI overlay (render-to-texture)
- **Create `src/engine/ui/overlay.*`**:
  - Define a small API for a renderable UI overlay (size, render target handle, visibility).
  - Engine owns composition; game owns HUD/Console implementation.
- **Move UI plumbing into engine**:
  - Move `src/game/ui/bridges/*` to `src/engine/ui/bridges/*`.
  - Move UI platform renderers (`renderer_bgfx/diligent/forge.*`) to `src/engine/ui/platform/*`.
  - Keep `src/game/ui/frontends/*`, `src/game/ui/controllers/*`, `src/game/ui/models/*` in game.
- **Integrate into EngineApp**:
  - EngineApp composes overlay output into render pipeline.
  - Game returns its overlay (HUD/Console) via `GameInterface`.

### Phase 3 — ECS core in engine (required)
- **Introduce `src/engine/ecs/*`**:
  - `ecs::World`, `EntityId`, `SystemGraph`, basic components (Transform, RenderMesh, Material).
  - Provide a minimal API for component registration and system updates.
- **Add ECS to EngineApp**:
  - EngineApp owns `ecs::World` and runs system graph each tick.
  - Expose world access to `GameInterface` for registration and updates.
- **Keep game object model intact**:
  - No gameplay logic rewrite yet; ECS is opt-in for now.

### Phase 4 — RenderSystem + adapters
- **Engine RenderSystem**:
  - Move frame begin/end and layer render orchestration into an engine system.
  - RenderSystem consumes ECS `Transform` + `RenderMesh` + `Material` components.
- **Game-to-ECS adapters**:
  - Add a thin adapter that mirrors existing game entity state into ECS components each frame (positions, mesh IDs, materials).
  - Start with world/static geometry and player tanks; expand to shots.
- **Keep game-specific rendering in game**:
  - Radar, FOV lines, HUD remain game-owned; consume engine APIs for overlays and render targets.

### Phase 5 — Networking cleanup
- **Move transports to engine**:
  - Consolidate `src/engine/network/*` and remove `src/game/net/backends/enet/*`.
  - Provide an engine transport interface for client/server.
- **Game keeps protocol**:
  - `src/game/net/messages.hpp`, `src/game/net/proto_codec.*`, `src/game/protos/messages.proto` stay in game.
  - Client/server network code becomes an adapter on top of engine transport.

### Phase 6 — Optional migrations
- Migrate physics or input mapping to ECS systems where beneficial.
- Move generic binding serialization to engine if it’s reused beyond BZ3.

## Optional next step

Before moving code, generate a target directory layout and a dependency graph (engine → game only) to confirm no game-specific types leak into engine modules.

## Alignment notes with bz3-karma

- `karma::app::EngineApp` matches `bz3-karma`’s engine runtime model.
- Overlay in karma maps to BZ3’s existing UI render-to-texture path.
- ECS is intentionally required earlier here to converge with karma’s `World` + systems model.
