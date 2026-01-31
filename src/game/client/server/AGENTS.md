# src/game/client/server/AGENTS.md

Read `src/game/client/AGENTS.md` first.
This directory contains **client-side networking helpers**: server discovery,
community list handling, and auth/connection flows.

## Key responsibilities
- Discover LAN servers and build server lists.
- Handle community list refreshes and credentials.
- Orchestrate join flow (including roaming mode selection).

## Key files
- `community_browser_controller.*`
  - Main state machine for the community browser panel.
- `community_auth_client.*`
  - Authentication flow for community servers.
- `server_connector.*`
  - Connection orchestration and error handling.
- `server_discovery.*`
  - LAN discovery and server list integration.

## Gotchas
- Join flow is stateful: keep selection and pending state consistent.
- Roaming mode is chosen at join time and cannot be toggled while connected.
