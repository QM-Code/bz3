# src/game/common/AGENTS.md

Read `src/game/AGENTS.md` first.
This directory contains **game-level shared utilities** and data-path specs.

## Key files
- `data_path_spec.*`
  - Declares game-specific data path layers for ConfigStore and asset lookup.

These are used by engine `data_path_resolver` to locate assets and configs.
