# src/engine/ui/platform/architecture.md

Each renderer_* file implements RmlUi's RenderInterface for a specific graphics
backend. The interface owns a render target and translates RmlUi geometry into
backend draw calls. Output textures are exposed to the game renderer via the
engine UI bridge.
