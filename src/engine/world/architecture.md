# src/engine/world/architecture.md

World content loading is backend-based. The engine selects a backend (fs) and
exposes content lookup to the game. Game code should not access filesystem
paths directly when an asset key is available.
