# src/engine/karma/geometry/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma geometry subsystem.
They map `karma/geometry/...` includes to the real engine headers under
`src/engine/geometry/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/geometry/`.
