# AGENTS-forge.md

This file captures the Forge backend debugging work performed in this session, including experiments, dead ends, and validated observations. It is meant to be a detailed handoff so a future agent can continue efficiently.

## Summary of the current problem
- **Forge backend renders only the blue background + HUD/UI overlay.**
- **No world geometry is visible**, even though meshes are loaded and draw calls are issued.
- **RmlUi Forge UI appears scrunched/bottom-left or blank** (varies with tests), while ImGui UI overlay draws correctly.
- The same world renders under **bgfx** and **diligent** backends.

## Environment and context
- Platform: **Wayland + Vulkan** on Linux.
- GPU: **NVIDIA GTX 970**.
- Initial crashes were due to **driver mismatch** (NVML version mismatch). A reboot fixed that.
- Vulkan validation layer is now installed and can be enabled with `KARMA_FORGE_ENABLE_VALIDATION=1`.

## High-level flow
- Render loop is in `src/game/renderer/render.cpp`:
  - `beginFrame()`
  - render radar layer to offscreen RT
  - render main layer to swapchain
  - UI overlay rendered after renderLayer
  - present
- Forge backend is `src/engine/graphics/backends/forge/backend.cpp`.

## Key observations from logs
- Forge initializes successfully, swapchain created, GPU detected, etc.
- Draw calls for meshes are logged (for both radar and main layers) but **nothing is visible**.
- `renderLayer` is definitely called for the main layer (verified by logs).
- Meshes are loaded from `/data/common/models/world.glb` and mesh bounds are sensible (within +/-100).
- A forced swapchain **magenta clear** from `renderLayer` works (meaning the render pass is active).

## Important debug flags added
- `KARMA_FORGE_DEBUG_CAMERA=1`: logs camera pos/rot + viewProj once per layer.
- `KARMA_FORGE_DEBUG_CLEAR_SWAPCHAIN=1`: forces magenta clear in `renderLayer` for swapchain pass.
- `KARMA_FORGE_DEBUG_MESH_TRI=1`: draw a clip-space triangle through mesh pipeline.
- `KARMA_FORGE_DEBUG_ONLY_TRI=1`: skip normal entity rendering.
- `KARMA_FORGE_DEBUG_SOLID_SHADER=1`: use a solid-color shader (no textures).
- `KARMA_FORGE_DEBUG_UI_QUAD=1`: draw a fullscreen magenta quad using the UI overlay pipeline inside `renderLayer`.
- `KARMA_FORGE_ENABLE_VALIDATION=1`: enable Vulkan validation layer (if installed).
- `KARMA_FORGE_DEBUG_SINGLE_DESCRIPTOR=1`: update descriptor set once per frame to avoid validation errors.
- `KARMA_FORGE_USE_LH=1`: switch to LH_ZO projection (test for handedness mismatch).
- `KARMA_DISABLE_UI_OVERLAY=1`: skip UI overlay draw.

## Dead ends / things that did NOT fix it
- Switching between **RH/LH projection** (no change).
- Drawing a triangle via mesh pipeline (never visible).
- Drawing a triangle via UI overlay pipeline inside `renderLayer` (never visible).
- Rendering mesh with solid shader (no texture sampling) (no change).
- Disabling UI overlay (screen is still blue or magenta when forced clear).
- Force blending off (render black, but still no geometry).

## Confirmed facts
- Swapchain clear in `renderLayer` shows up when forced, proving `renderLayer` runs.
- RmlUi/ImGui overlay pipeline draws successfully **outside** `renderLayer` (via `renderUiOverlay`), so swapchain + command buffer path works in general.
- World model loads and meshes are created (`setEntityModel` logs show 15 meshes).
- Mesh bounds for world model are reasonable (within +/-100).

## Critical validation errors seen
When validation was enabled (after installing validation layer), these errors appeared:

1) **Image layout mismatch**:
   - `vkQueueSubmit(): ... image layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` while expecting SHADER_READ.
   - This was traced to `whiteTexture_` being created in COPY_DEST and never transitioned.
   - Fix added: explicit transition of `whiteTexture_` to SHADER_RESOURCE at start of `renderLayer`.

2) **Fence in signaled state**:
   - `vkAcquireNextImageKHR(): fence submitted in SIGNALED state`.
   - Fix added: `acquireNextImage` now uses `nullptr` fence (avoid reuse).

3) **Pipeline barrier inside dynamic rendering**:
   - `vkCmdPipelineBarrier`: cannot be called inside dynamic rendering instance.
   - Root cause: render targets were bound in `beginFrame`, then later barriers executed.
   - Fix added: removed `cmdBindRenderTargets` from `beginFrame`.
   - Also added `cmdBindRenderTargets(cmd, nullptr)` to end passes in `renderLayer` and `renderBrightnessPass`.

4) **Descriptor set updated while bound**:
   - `vkCmdEndRendering`: descriptor set was destroyed or updated while bound.
   - Root cause: descriptor sets updated many times per frame.
   - Fix added: descriptor sets now use a small ring (size 3), and per-frame set index.
   - Optional flag `KARMA_FORGE_DEBUG_SINGLE_DESCRIPTOR=1` forces a single update to avoid validation issues.

## Current code changes (summary)
These changes are already in the repo and may need cleanup later:

### backend.cpp
- `computeProjectionMatrix()` uses RH_ZO by default; optional LH_ZO via `KARMA_FORGE_USE_LH=1`.
- Removed render target binding in `beginFrame` to avoid dynamic rendering conflicts.
- Added explicit transition of `whiteTexture_` from COPY_DEST to SHADER_RESOURCE.
- Added debug triangle (mesh pipeline) and UI quad (UI pipeline) options.
- Added mesh bounds logging via `KARMA_FORGE_DEBUG_MESH_BOUNDS=1`.
- Added swapchain clear override via `KARMA_FORGE_DEBUG_CLEAR_SWAPCHAIN=1`.
- Added ring descriptor set support (`kDescriptorSetRingSize=3`) for mesh, ui overlay, brightness.
- Added `cmdBindRenderTargets(cmd, nullptr)` after render targets or brightness pass.

### Shaders
- Added debug mesh shaders:
  - `data/forge/shaders/mesh_debug.vert`
  - `data/forge/shaders/mesh_debug.frag`

## What can be ruled out
- **Wrong world model path**: world.glb is correct (from config), loaded and parsed.
- **Camera positions**: debug logs show sensible camera values for main layer.
- **Swapchain render pass not running**: forced magenta clear is visible.

## Current suspicion
The issue appears to be **within the Forge mesh pipeline** path. Even the mesh debug triangle (clip-space, solid shader, no textures) does not render. The UI overlay pipeline works when used in `renderUiOverlay`, but fails when used inside `renderLayer` (debug UI quad).

This points to:
- **Dynamic rendering state / render pass lifecycle** still being broken, or
- **Descriptor set lifetime / update timing** still causing invalid binds, or
- **Some pipeline state mismatch** (vertex layout binding index vs actual binding).

The validation errors suggest the backend is still violating Vulkan rules; fix those first to trust the render results.

## Next recommended steps
1) **Re-run validation with current fixes** and see if errors persist.
   - Use:
     - `KARMA_FORGE_ENABLE_VALIDATION=1`
     - `KARMA_FORGE_DEBUG_SINGLE_DESCRIPTOR=1`
     - `KARMA_FORGE_DEBUG_SOLID_SHADER=1`
     - `KARMA_FORGE_DEBUG_MESH_TRI=1`
     - `KARMA_FORGE_DEBUG_ONLY_TRI=1`

2) If errors persist, prioritize:
   - Fix any remaining dynamic rendering state errors (ensure `cmdBindRenderTargets(nullptr)` is called at all exit points).
   - Fix descriptor set update order (only update before bind, never while bound).

3) Once validation is clean, retest:
   - Mesh debug triangle (clip space) should appear.
   - If it does, re-enable normal mesh draw and re-test world.

4) If mesh triangle still fails with clean validation:
   - Compare mesh pipeline creation with The Forge examples (especially vertex layout + binding index).
   - Double-check that `cmdBindVertexBuffer` uses correct binding index for the mesh pipeline.

## Commands used frequently
- Build:
  - `./bzbuild.py sdl3 imgui jolt sdlaudio forge enet fs`
- Run with debug flags:
  - `KARMA_FORGE_DEBUG_CLEAR_SWAPCHAIN=1 ./build-sdl3-imgui-jolt-sdlaudio-forge-enet-fs/bz3`
  - `KARMA_FORGE_ENABLE_VALIDATION=1 ./build-sdl3-imgui-jolt-sdlaudio-forge-enet-fs/bz3 -v`
  - `KARMA_FORGE_DEBUG_SOLID_SHADER=1 KARMA_FORGE_DEBUG_MESH_TRI=1 KARMA_FORGE_DEBUG_ONLY_TRI=1 ./build-sdl3-imgui-jolt-sdlaudio-forge-enet-fs/bz3`

## Known files touched during session
- `src/engine/graphics/backends/forge/backend.cpp` (many debug flags and fixes)
- `data/forge/shaders/mesh_debug.vert`
- `data/forge/shaders/mesh_debug.frag`

---
If you revisit this later, start by verifying validation errors are gone with the latest fixes, then focus on rendering the debug triangle in the mesh pipeline. If that shows, the world should be visible with correct camera; if not, the pipeline state is still invalid.
