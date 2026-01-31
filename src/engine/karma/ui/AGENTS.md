# src/engine/karma/ui/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma ui subsystem.
They map `karma/ui/...` includes to the real engine headers under
`src/engine/ui/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/ui/`.
