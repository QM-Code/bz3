# src/engine/input/architecture.md

Input flow:

1) Platform collects events (window backend).
2) `Input` updates per-frame event list.
3) `InputMapper` checks bindings against events.
4) Game queries actions (triggered or held).

Bindings are stored in `keybindings` config and parsed by `mapping/`.
