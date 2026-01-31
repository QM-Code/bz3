# src/engine/karma/network/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma network subsystem.
They map `karma/network/...` includes to the real engine headers under
`src/engine/network/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/network/`.
