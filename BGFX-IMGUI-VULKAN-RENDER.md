# BGFX + ImGui + Vulkan rendering notes

These notes summarize the current bgfx rendering work, how the scene is now loading/playing in monochrome, and what’s left to render real assets.

## Goal
- Bring up a **bgfx Vulkan** rendering backend (Wayland-first) as an alternative to threepp/Filament.
- Keep game logic playable while rendering is incrementally implemented.

## Current status
- bgfx initializes (renderer=9) on Wayland with Vulkan.
- The game is playable (movement, shots, HUD/console via ImGui).
- **World surfaces render in monochrome** using a very simple shader.
- A basic triangle test and a simple mesh path are working.
- No textured/colored objects yet (tanks/buildings/shots not visible with real materials).

## How it works right now
- **Meshes** are loaded via `engine/geometry/mesh_loader` into a basic `graphics::MeshData`.
- `graphics_backend::BgfxBackend` creates a vertex/index buffer for each mesh.
- Rendering uses a **single color shader** and an `u_color` uniform.
- Everything is effectively “flat shaded,” no textures, no lighting.

## Key files involved
- `src/engine/graphics/backends/bgfx/backend.cpp`
  - bgfx init, mesh buffer creation, renderLayer implementation.
  - test triangle + mesh shader build/load.
- `data/bgfx/shaders/`
  - `vs_triangle.sc` / `fs_triangle.sc`
  - `mesh/vs_mesh.sc` / `mesh/fs_mesh.sc`
  - `imgui/vs_imgui.sc` / `imgui/fs_imgui.sc`

## How to run (bgfx Vulkan)
```
SDL_VIDEODRIVER=wayland BZ3_BGFX_NO_GL=1 ./build-sdl-imgui-jolt-sdlaudio-bgfx-enet-fs/bz3 -v
```

## What is missing
- **Material system** for bgfx (albedo, normal, metallic/roughness, etc).
- **Texture loading** and binding.
- **Mesh attributes beyond position** (normals, UVs, tangents).
- **Lighting** (directional/ambient at minimum).
- **Entity materials** for tanks, buildings, shots.

## Suggested next steps
1) **Expand mesh vertex format**
   - Add normals + UVs (and tangents if needed).
   - Update mesh loader and bgfx vertex layout accordingly.

2) **Implement texture loading**
   - Load albedo textures from assets.
   - Bind textures in bgfx draw calls.

3) **Add basic lighting shader**
   - Simple directional + ambient lighting.
   - Use normals to shade meshes.

4) **Hook up material data**
   - Map existing game material info to bgfx uniforms/textures.

5) **Render core objects**
   - Tanks, buildings, shots, world objects.
   - Verify visibility/culling, transforms, and alpha handling.

6) **Radar/UI integration**
   - Ensure radar targets render (or keep the current 2D overlay path).

## Notes
- Current monochrome rendering shows the pipeline is alive, but it is purely a placeholder.
- The next agent should focus on **vertex formats + textures + simple lighting** to make real objects visible.

