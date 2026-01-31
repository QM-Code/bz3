# src/engine/karma/physics/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma physics subsystem.
They map `karma/physics/...` includes to the real engine headers under
`src/engine/physics/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/physics/`.
