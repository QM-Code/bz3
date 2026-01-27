# BGFX + ImGui + Vulkan (Wayland) crash notes

These notes summarize the recent bgfx/ImGui/Vulkan work and the font-related crashes, for the next agent.

## Goal
- Move BZ3 toward **Wayland + Vulkan first** (bgfx backend), with OpenGL/X11 as fallback.
- Use **SDL3** for windowing and **bgfx** for rendering.
- Use **ImGui** for UI on bgfx (RmlUi stays for other backends).

## Current working run command (Wayland + bgfx)
```
SDL_VIDEODRIVER=wayland BZ3_BGFX_NO_GL=1 ./build-sdl-imgui-jolt-sdlaudio-bgfx-enet-fs/bz3 -v
```

## What works
- bgfx Vulkan init on Wayland works (renderer=9).
- ImGui bgfx renderer works (UI renders) with **simple fonts** and when no runtime font rebuild is attempted.
- 3D world is still minimal (monochrome surface + triangle test) but the pipeline is live.

## Regression: Vulkan + fonts
- **Before enabling SDL Vulkan feature**, bgfx+ImGui worked with Latin/European languages; complex scripts were flaky.
- **After enabling SDL Vulkan feature**, ImGui began segfaulting during font atlas build (`io.Fonts->Build()`), especially for non‑Latin languages.
- Segfaults manifest at startup, often right after:
  - `UiSystem: ImGui font atlas build start`

## Partial fixes attempted
1) **Shutdown order fixes** (quit segfault)
   - `src/engine/graphics/backends/bgfx/backend.cpp`: call `bgfx::frame()` before `bgfx::shutdown()`; set `initialized=false`, `testReady=false`.
   - `src/game/ui/backends/imgui/backend.cpp`: guard `shutdownBgfxRenderer()` with `if (!bgfx::getCaps()) return;`.
   - Quit segfault resolved.

2) **Fallback font reduction (important)**
   - `src/game/ui/backends/imgui/console/console.cpp`:
     - Fallbacks no longer always include Cyrillic.
     - Only add fallback fonts for the **active language** plus Latin.
   - This *greatly* improved stability for non‑Latin languages.

3) **Disable live language reload for bgfx**
   - `src/game/ui/backends/imgui/backend.cpp`: language change callback now warns and returns for bgfx builds (restart required).
   - Rationale: runtime atlas rebuild in bgfx path causes crashes.

## Still failing / inconsistent
- After the fallback change, complex languages are much more stable, but crashes can still occur for some languages.
- Eventually, after disabling live reload, **easy languages (English) began crashing at atlas build**.
- Latest crash example:
  - `UiSystem: ImGui font atlas build start` -> segfault.

## Hypothesis
- The crash is tied to **font atlas building** with large glyph ranges and/or FreeType builder in bgfx Vulkan path.
- Atlas rebuilds (or large ranges) are the risky part.
- The “right” long-term solution is to **avoid runtime rebuilds** and/or use **script‑specific atlases**.

## Environment flags used
- `BZ3_BGFX_NO_GL=1` — skip OpenGL init (Vulkan only).
- `BZ3_BGFX_SIMPLE_FONTS=0/1` — simplify font atlas in bgfx builds.
- `BZ3_BGFX_ALLOW_COLOR_FONTS=1` — allow color fonts (disabled by default for bgfx).
- `BZ3_BGFX_ALLOW_EMOJI_FONTS=1` — allow emoji fonts (disabled by default for bgfx).
- `BZ3_BGFX_ALLOW_COMPLEX_FONTS=1` — previously used to allow complex fonts (guard removed later).

## Key files touched
- `src/engine/graphics/backends/bgfx/backend.cpp`
- `src/game/ui/backends/imgui/backend.cpp`
- `src/game/ui/backends/imgui/console/console.cpp`
- `src/engine/platform/backends/window_sdl.cpp`
- `src/game/client/main.cpp`

## Suggested next steps
- Investigate **ImGui FreeType builder** on bgfx Vulkan path.
- Consider **per-language font atlases** and require restart for language changes.
- Consider **script detection** in chat/server text and preloading relevant fonts at startup.
- If crashes persist, consider disabling FreeType builder only for bgfx (fallback to stb_truetype if supported), or shipping prebuilt font atlases.

