# src/engine/karma/core/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma core subsystem.
They map `karma/core/...` includes to the real engine headers under
`src/engine/core/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/core/`.
