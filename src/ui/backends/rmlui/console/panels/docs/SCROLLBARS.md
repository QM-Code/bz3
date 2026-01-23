# RmlUi Scrollbar Checklist (BZ3)

Use this pattern whenever you need a scrollable area. It centralizes the "gotchas" that make scrollbars appear reliably in RmlUi.

## Required CSS helpers (already in `data/client/ui/console.rcss`)
- `.scrollable` (from `data/client/ui/rmlui_scrollbars.rcss`)
- `.scroll-frame` (overflow + box-sizing)
- `.scroll-content` (inner width + right padding)
- `.scroll-fill` (flex sizing + min-height 0 + height 100%)

## Minimal markup pattern
```rml
<div class="scrollable scroll-frame scroll-fill">
    <div class="scroll-content">
        <!-- long content here -->
    </div>
</div>
```

## Flex chain requirements (most common failure)
- Every flex parent between the panel root and the scroll-frame must allow the child to shrink:
  - `min-height: 0;`
  - and if needed `flex: 1 1 auto;`
- Without this, the scrollable never gets a fixed height and no scrollbar is generated.

## Visual scrollbar styling
Scrollbar styles are centralized in `data/client/ui/rmlui_scrollbars.rcss` under `.scrollable`.
If the scrollbar is invisible, confirm:
1) The element has `overflow-y: auto/scroll`
2) The element has a constrained height (see flex chain requirements)
3) `.scrollable` class is present so the scrollbar elements are styled

## Known-good examples
- Community panel server list + server details
- Start Server panel log output + running servers list
- Settings panel bindings list
