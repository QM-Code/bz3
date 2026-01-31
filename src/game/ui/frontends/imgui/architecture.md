# src/game/ui/frontends/imgui/architecture.md

ImGui frontend flow:
1) Backend begins ImGui frame.
2) HUD draws using shared models.
3) Console draws panels and uses controllers.
4) Render output is sent to engine UI bridge.
