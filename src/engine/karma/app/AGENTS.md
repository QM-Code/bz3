# src/engine/karma/app/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma app subsystem.
They map `karma/app/...` includes to the real engine headers under
`src/engine/app/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/app/`.
