# src/game/ui/frontends/rmlui/console/panels/architecture.md

Panel architecture:
- Each `panel_*` class derives from the RmlUi panel base.
- Panels bind RML elements by ID and register event listeners.
- Controllers handle config/model I/O; panels are primarily view logic.
- The backend instantiates panels and forwards lifecycle events.
