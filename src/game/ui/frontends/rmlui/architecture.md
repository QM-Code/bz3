# src/game/ui/frontends/rmlui/architecture.md

RmlUi frontend flow:
1) Backend creates RmlUi context and render interface.
2) Loads HUD and console documents from `data/client/ui/`.
3) Panels update shared models and respond to input.
4) Output texture is exposed through the engine UI bridge.
