# src/game/ui/controllers/architecture.md

Controllers sit between ConfigStore and UI models. They encapsulate business
logic for settings, bindings, and console state so that ImGui and RmlUi can
share behavior.
