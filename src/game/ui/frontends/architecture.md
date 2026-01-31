# src/game/ui/frontends/architecture.md

Both frontends implement the same backend interface. They render HUD and
Console using shared models. Input events are mapped into each UI framework’s
native system via shared input mapping helpers.
