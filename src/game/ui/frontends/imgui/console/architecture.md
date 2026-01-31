# src/game/ui/frontends/imgui/console/architecture.md

ImGui console flow:
- Console view owns tab state and panel routing.
- Panels draw UI and interact with controllers.
- Selection/refresh state is stored in shared models.
