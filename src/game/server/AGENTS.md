# src/game/server/AGENTS.md

Read `src/game/AGENTS.md` first.
This directory implements the **server runtime** for BZ3.

## Responsibilities
- Server entrypoint and lifecycle
- World session management
- Plugins and admin commands

## Key files
- `main.cpp`
  - Server entry point.

- `world_session.*`
  - Server-side world state and tick.

- `plugin.*`
  - Python plugin hooks and callback registration.

- `terminal_commands.*`
  - CLI command processing for server.

## How it connects
- Uses engine physics and network transport.
- Uses game network protocol for messages.
