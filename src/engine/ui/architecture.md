# src/engine/ui/architecture.md

Engine UI architecture:
- Backends (bgfx/diligent) provide texture targets for UI output.
- RmlUi and ImGui render into these targets.
- Game renderer composites UI textures after 3D scene.

Engine UI layer is a bridge; all UI logic is in `src/game/ui/`.
