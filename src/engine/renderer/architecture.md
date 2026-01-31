# src/engine/renderer/architecture.md

Renderer core flow:
1) Game updates `RendererContext` (camera, layers, lighting).
2) `RendererCore` prepares device state and render targets.
3) `SceneRenderer` draws render entities using graphics backend.
4) UI overlays are composed by game/engine bridge.

The engine renderer is deliberately minimal; game code decides what to render.
