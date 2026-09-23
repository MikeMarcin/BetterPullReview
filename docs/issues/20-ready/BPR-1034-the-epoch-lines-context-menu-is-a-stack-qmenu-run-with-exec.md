---
schema: "implement-rfc.card.v1"
id: "BPR-1034"
title: "The epoch line's context menu is a stack QMenu run with exec(), so a window destroyed while it is open frees a stack object"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-1028 sweep for the same pattern, bpr-main-win","found_at":"e08a0409","related_cards":["BPR-1028"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1033--BPR-1034--6684dcafdd51.json"}
---

# BPR-1034: The epoch line's context menu is a stack QMenu run with exec(), so a window destroyed while it is open frees a stack object

## Defect

`epoch_line::contextMenuEvent` (`src/adapters/qt/src/epoch_line.cpp`) builds
`QMenu menu{ this }` on the stack, parented to the epoch line, adds "Compare
from here", "Compare to here", "Show this update alone" and "Copy head
commit", and runs `menu.exec(aEvent->globalPos())`.

This is the shape [[BPR-1028]] removed from the Windows strip menu. There, an
accessible File > Quit destroyed the window inside the menu's nested loop.
The window deleted the stack menu as one of its children, and the Debug CRT
asserted `_CrtIsValidHeapPointer(block)`. None of the epoch line's own four
actions destroys the window. But anything else that destroys the window
while this menu is open frees a stack object the same way, on every
platform: an accessible Quit or window close, a profile shutdown, or an
update-driven quit. `contextMenuEvent` then returns into a destroyed
`epoch_line`.

Not reproduced. Found by reading the code while fixing [[BPR-1028]].

## Scope

- In Qt presentation only. Open the menu as a heap child that deletes itself
  on close, with `popup()`, as `workbench_window::open_strip_menu` does. Its
  actions' connections capture the update index by value, so they need no
  frame of `contextMenuEvent`.
- Keep the object names (`epochLineMenu`, `compareFromUpdateAction`,
  `compareToUpdateAction`, `showUpdateAloneAction`, `copyUpdateHeadAction`),
  the enabled rules and the semantic `choose` calls.
- Adjust any course that relied on `exec()` blocking.

## Acceptance

- AC1 An offscreen course opens the epoch line's context menu and destroys
  its window while the menu is open, and passes: no CRT report, no popup left
  behind, and the menu destroyed with its window. Against the stack form, the
  same course fails, at least under the MSVC Debug CRT.
- AC2 The epoch line's existing courses pass on macOS, Linux and Windows.
