# src/engine/karma/platform/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma platform subsystem.
They map `karma/platform/...` includes to the real engine headers under
`src/engine/platform/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/platform/`.
