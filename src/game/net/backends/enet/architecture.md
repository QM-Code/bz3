# src/game/net/backends/enet/architecture.md

ENet transport implements a reliable UDP channel for the game protocol. It
exposes a backend-agnostic interface used by `src/game/net/`.
