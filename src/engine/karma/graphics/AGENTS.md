# src/engine/karma/graphics/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma graphics subsystem.
They map `karma/graphics/...` includes to the real engine headers under
`src/engine/graphics/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/graphics/`.
