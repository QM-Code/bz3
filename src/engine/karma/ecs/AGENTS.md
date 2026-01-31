# src/engine/karma/ecs/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma ecs subsystem.
They map `karma/ecs/...` includes to the real engine headers under
`src/engine/ecs/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/ecs/`.
