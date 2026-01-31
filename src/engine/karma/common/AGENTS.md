# src/engine/karma/common/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma common subsystem.
They map `karma/common/...` includes to the real engine headers under
`src/engine/common/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/common/`.
