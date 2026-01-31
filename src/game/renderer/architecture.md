# src/game/renderer/architecture.md

The game renderer wraps the engine renderer core and provides game-specific
features:
- Render ID management
- ECS entity registration for meshes
- Camera control
- Radar render target and overlay elements

Radar uses an offscreen target that is composited by the HUD.
