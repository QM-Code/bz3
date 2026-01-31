# src/game/protos/AGENTS.md

Read `src/game/net/AGENTS.md` first.
This directory contains **protobuf schema** for the game protocol.

## Workflow for changes
1) Update `messages.proto`.
2) Regenerate protobuf code if needed (build scripts handle this).
3) Update `proto_codec.*` and any message handlers in client/server.

## Gotchas
- Schema changes often require versioning or compatibility handling.
- Keep enums and field IDs stable when possible.
