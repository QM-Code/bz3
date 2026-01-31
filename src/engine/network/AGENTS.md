# src/engine/network/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory provides the **transport layer** (ENet) used by the game’s
protocol.

## Scope
- Engine handles transport sockets and packet send/receive.
- Game code defines the protocol, message framing, and reliability semantics.

## Key files
- `enet_transport.*`
  - ENet-based transport implementation.

- `transport_factory.*`
  - Selects the transport backend (currently ENet only).

## How it connects to game code
- Game networking (`src/game/net/`) uses the transport to move bytes.
- Game handles all message encoding/decoding.

## Gotchas
- Keep this layer protocol-agnostic.
- Avoid adding BZ3 message types here.
