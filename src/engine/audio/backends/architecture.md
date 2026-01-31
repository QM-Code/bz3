# src/engine/audio/backends/architecture.md

Backends implement the engine audio interface. The factory selects one at build
compile-time. Game code never sees these classes directly.
