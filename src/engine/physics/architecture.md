# src/engine/physics/architecture.md

## Structure
- Wrapper types (`PhysicsWorld`, `RigidBody`, `StaticBody`, `PlayerController`).
- Backend factory chooses a concrete backend (Jolt or PhysX).
- Game code only uses wrapper types.

## Flow
Game → PhysicsWorld → Backend → Physics SDK
