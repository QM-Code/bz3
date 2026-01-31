# src/engine/karma/world/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma world subsystem.
They map `karma/world/...` includes to the real engine headers under
`src/engine/world/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/world/`.
