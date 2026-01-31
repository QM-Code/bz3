# src/engine/platform/architecture.md

Platform flow:
- Backend creates window and polls events.
- Events are normalized into engine types.
- Input/Ui systems consume those events.

The platform layer should stay minimal and reusable.
