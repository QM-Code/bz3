# src/engine/common/architecture.md

## Config flow
- ConfigStore loads layered JSON files (engine defaults, game defaults, user).
- `ReadRequired*` helpers enforce required keys (fatal on missing).
- Config changes are revisioned and can be watched by UI or systems.

## Data path resolution
- `KARMA_DATA_DIR` sets the root.
- Data path resolver builds asset lookup tables from layered configs.

## i18n
- Language JSON files are loaded by `i18n`.
- Game UI uses string keys rather than hardcoded labels.
