# src/engine/input/mapping/architecture.md

The mapping layer translates key/mouse strings into concrete bindings, stores
per-action lists, and evaluates input events. It is pure engine logic and does
not know about game actions beyond their string IDs.
