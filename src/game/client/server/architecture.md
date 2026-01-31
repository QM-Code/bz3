# src/game/client/server/architecture.md

This layer sits between UI and network:
- UI selects a server and triggers join/roam requests.
- The controller resolves credentials, handles auth, and drives connection.
- Network transport handles bytes; protocol lives in `src/game/net/`.
