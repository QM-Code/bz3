# src/engine/karma/audio/AGENTS.md

Read:
- `src/engine/AGENTS.md`
- `src/engine/karma/AGENTS.md`

This directory contains **forwarder headers** for the Karma audio subsystem.
They map `karma/audio/...` includes to the real engine headers under
`src/engine/audio/`.

There should be **no implementation** here. If you need to change engine logic,
edit the real files under `src/engine/audio/`.
