# src/game/renderer/architecture.md

The renderer wraps the engine renderer core and provides game-owned features:
- Radar-only render ID management
- Camera control
- Radar render target and overlay elements

Radar uses an offscreen target that is composited by the HUD.
