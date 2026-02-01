# Radar Issues + Context (BZ3)

This note captures the current radar behavior, the recent refactor context, and
what we learned while debugging. It is intended to onboard another agent so they
can pick up the radar work later without redoing the investigation.

## High-level context
- We are in the middle of the engine/game refactor (see `NEW-ENGINE-REFACTOR.md`).
- The engine is moving toward Karma’s model: engine owns loop/systems; game owns
  gameplay + UI logic.
- **Radar is game-specific**. The engine should not know about “radar”.
- Radar rendering currently lives in `src/game/renderer/` (game-owned), and uses
  engine graphics APIs to render an offscreen target that the HUD displays.

## Current radar behavior
- Radar shows player dot, FoV lines, other players (dots), shots (dots), and
  buildings as top-down silhouettes.
- The radar ground looks like the in-game ground texture, which is surprising.

## What we tested
- We hard-coded the radar fragment shader (`data/common/shaders/radar.frag`) to
  output a solid magenta. The radar **did not** turn magenta under either BGFX
  or Diligent builds.
- This means the shader source file is **not** used at runtime by either backend.
- We confirmed the HUD is showing the radar render target (texture id matches
  the radar RT).

## Why the magenta didn’t show
- The BGFX backend does **not** use `MaterialDesc.vertexShaderPath` or
  `MaterialDesc.fragmentShaderPath` when drawing meshes. It stores the material
  but the render path always uses the built-in `meshProgram` shaders.
- Diligent appears to behave similarly for this path (radar still showed ground
  textures after shader change).
- Net effect: the radar render pass is just rendering the actual world mesh with
  its real texture in the radar pass. That’s why the radar ground looks like the
  in-game ground.

## Relevant files
- Game radar renderer:
  - `src/game/renderer/radar_renderer.*`
  - `src/game/renderer/renderer.*` (radar sync + target)
  - `src/game/client/game.cpp` (calls `setRadarShaderPath`)
- Shader sources:
  - `data/common/shaders/radar.vert`
  - `data/common/shaders/radar.frag`
- BGFX backend (material ignores shader path):
  - `src/engine/graphics/backends/bgfx/backend.cpp`
- HUD wiring:
  - RmlUi: `src/game/ui/frontends/rmlui/hud/radar.cpp`
  - ImGui: `src/game/ui/frontends/imgui/hud/radar.cpp`

## Outcome of the investigation
- The radar shader paths are currently a no-op for BGFX and effectively no-op
  for Diligent in this usage.
- The radar ground texture is not a separate radar texture at all; it is just
  the world mesh’s actual texture rendered into the radar target.

## Options to achieve “solid color ground” (not implemented)
Pick one later; do NOT do this now during the refactor unless needed.

1) **Backend-side quick change (lowest effort)**
   - In BGFX backend, when `radarPass == true`, skip binding mesh textures and
     force a constant color/alpha (e.g. green @ 0.35).
   - This affects all radar geometry (including buildings) unless extra logic
     is added.

2) **Selective radar material override (moderate effort)**
   - Add a flag/component for radar-only “solid fill”.
   - Apply constant color/alpha only to the world mesh entity in the radar pass
     (keep dots/silhouettes as-is).

3) **Game-side quad/mesh for ground (clean separation)**
   - Do not render world mesh for the radar background.
   - Instead render a single quad/circle in the radar pass as the ground.
   - This avoids shader-path support and keeps radar fully game-owned.

## Notes about refactor changes that touched radar
- Radar is now explicitly game-owned. It lives under `src/game/renderer/`.
- The ECS tag for radar sync is now game-owned:
  - `src/game/renderer/radar_components.hpp` defines `game::renderer::RadarRenderable`.
  - Engine ECS components no longer contain `RadarRenderable`.
- The renderer wrapper in `src/game/renderer/` still handles radar sync and
  rendering into the offscreen target used by the HUD.

