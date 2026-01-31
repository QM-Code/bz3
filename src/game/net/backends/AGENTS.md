# src/game/net/backends/AGENTS.md

Read `src/game/net/AGENTS.md` first.
This directory contains **network transport backends** for the game protocol.

## Backend
- `enet/` — ENet-based client/server transport.

## Selection
Backend is selected at build time via `KARMA_NETWORK_BACKEND`.
