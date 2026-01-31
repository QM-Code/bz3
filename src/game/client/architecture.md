# src/game/client/architecture.md

Client flow:
1) `main.cpp` builds the engine and starts client loop.
2) `Game` creates world session and player (unless roaming).
3) `Player` updates camera position/rotation each frame.
4) `RoamingCameraController` applies a free camera when roaming.
5) UI is driven by engine UI system; game handles chat input.
