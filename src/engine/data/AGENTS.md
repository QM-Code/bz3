# src/engine/data/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory contains **engine-level default config** that is layered below
any game config.

## Purpose
- Provide engine defaults (e.g., platform settings, graphics defaults).
- These are merged by ConfigStore before game and user config layers.

## How to use
- Add or update defaults here when a required engine config key is introduced.
- Do **not** put game-specific values here.

## Related code
- `src/engine/common/config_store.*` reads this as part of the config layers.
