# src/engine/ecs/architecture.md

ECS is a lightweight storage layer:
- `World` owns entity IDs and component stores.
- Components are plain data.
- Systems are optional and live in `systems/`.

The ECS is currently used primarily for renderer integration.
