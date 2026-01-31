# src/engine/karma/renderer/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma renderer subsystem.
They map `karma/renderer/...` includes to the real engine headers under
`src/engine/renderer/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/renderer/`.
