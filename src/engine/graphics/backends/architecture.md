# src/engine/graphics/backends/architecture.md

Each backend is a self-contained GPU implementation. The engine renderer talks
through a shared interface, but each backend owns its own pipeline state,
resource creation, and API-specific details.

UI bridges are implemented per backend so ImGui/RmlUi can render into textures
that the renderer later composites.
