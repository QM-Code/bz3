# src/engine/karma/input/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma input subsystem.
They map `karma/input/...` includes to the real engine headers under
`src/engine/input/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/input/`.
