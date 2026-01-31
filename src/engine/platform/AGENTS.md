# src/engine/platform/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory defines the **platform abstraction**: window management, events,
input polling, and platform glue.

## Key files
- `window.*`
  - Engine window abstraction (create, poll, input state queries).

- `window_factory.cpp`
  - Build-time backend selection (SDL3 or stub).

- `events.*`
  - Engine event types used by input mapping and UI.

## Backends
- `backends/` contains the concrete SDL implementation(s).

## How it connects to the game
- Engine creates the window and polls events.
- UI and input systems consume platform events each frame.
