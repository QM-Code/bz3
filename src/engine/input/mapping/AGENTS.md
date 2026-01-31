# src/engine/input/mapping/AGENTS.md

Read `src/engine/input/AGENTS.md` first.
This directory contains the **binding parser and mapper** implementation.

## Components
- `binding.*`
  - Converts strings like "LEFT_MOUSE" or "W" into internal binding structs.
  - Also provides reverse mapping for display.

- `map.*`
  - Stores action → list of bindings.
  - Merges config with defaults; validates types.

- `mapper.*`
  - Evaluates input events against bindings.
  - Provides `actionTriggered` and `actionDown` logic.

## Key behavior
- If config for an action is missing, defaults are used.
- Unknown binding strings are logged and ignored.
