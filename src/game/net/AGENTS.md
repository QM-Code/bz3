# src/game/net/AGENTS.md

Read `src/game/AGENTS.md` first.
This directory defines the **BZ3 network protocol** and message-level networking.

## Responsibilities
- Protocol types and message encoding/decoding
- Client/server message routing
- Backend selection for transports (ENet)

## Key files
- `messages.*`
  - High-level message structs.

- `proto_codec.*`
  - Encode/decode between protobuf and internal structs.

- `backend_factory.*`
  - Selects network backend (ENet).

- `backends/enet/`
  - Concrete ENet client/server transport.

## How it connects
- Client and server world sessions use these messages.
- Engine transport layer only moves raw packets; this directory defines the
  message framing and meaning.

## Gotchas
- Keep protocol definitions in sync with `protos/messages.proto`.
- Any new message must update codec + client/server handling.
