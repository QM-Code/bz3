# src/engine/physics/backends/architecture.md

Each backend adapts a physics SDK to the engine’s physics interfaces. Backend
code should not leak into game logic; use the wrapper classes in `physics/`.
