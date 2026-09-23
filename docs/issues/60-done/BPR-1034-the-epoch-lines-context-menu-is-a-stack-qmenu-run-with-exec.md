---
schema: "implement-rfc.card.v1"
id: "BPR-1034"
title: "The epoch line's context menu is a stack QMenu run with exec(), so a window destroyed while it is open frees a stack object"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-1028 sweep for the same pattern, bpr-main-win","found_at":"e08a0409","related_cards":["BPR-1028"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1033--BPR-1034--6684dcafdd51.json"}
acceptance: [{"criterion":"AC1 An offscreen course opens the epoch line's context menu and destroys its window while the menu is open, and passes: no CRT report, no popup left behind, and the menu destroyed with its window. Against the stack form, the same course fails, at least under the MSVC Debug CRT","outcome":"pass","revision":"846b4851","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2, offscreen: test_epoch_line_menu_goes_with_its_window in qt_epoch_comparison_scale_*. Against the old stack exec() menu qt_epoch_comparison_scale_100 fails: debug_heap.cpp(904) Assertion failed: _CrtIsValidHeapPointer(block), (908) is_block_type_valid, child 0xC0000005 (SegFault). With the heap child and popup(): _100/_125/_200 pass (2.10-2.47 s) and _100 passes 5 of 5 with --repeat until-fail:5, with the window, control, line and menu destroyed and no active popup left.","path":"docs/issues/60-done/BPR-1034-the-epoch-lines-context-menu-is-a-stack-qmenu-run-with-exec.md#fixed-with-the-course-and-its-negative-control-bpr-main-win-2026-09-23"}},{"criterion":"AC2 The epoch line's existing courses pass on macOS, Linux and Windows","outcome":"pass","revision":"846b4851","evidence":{"kind":"command","detail":"Windows: qt_epoch_comparison_scale_100/125/200 (the epoch line's courses, including the click, drag and key selection and cached-head paging courses) pass at the fix. macOS and Linux: not run from this station; the product change is platform-neutral Qt (a heap QMenu with WA_DeleteOnClose and popup() in place of a stack QMenu with exec()), no existing course sent the line a context-menu event, and the new course is the same on every platform."}}]
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

## Fixed, with the course and its negative control (bpr-main-win, 2026-09-23)

**The fix.** In `epoch_line::contextMenuEvent`
(`src/adapters/qt/src/epoch_line.cpp`), the menu is now a heap child of the
line with `WA_DeleteOnClose`, opened with `popup()`. This is the shape
[[BPR-1028]] gave `workbench_window::open_strip_menu`. The object names, the
enabled rules and the `choose` calls are unchanged. The actions'
connections carry the update index by value and use the line as their
context, so they need nothing from the handler's frame. No course relied on
`exec()` blocking: nothing else in `tests/` sends the line a context-menu
event.

**The course.** `test_epoch_line_menu_goes_with_its_window`
(`tests/adapters_qt/epoch_comparison_control_tests.cpp`) runs in every
`qt_epoch_comparison_scale_*` row, on every platform. It places the epoch
comparison control in a window, publishes three updates, and sends the line
a context-menu request on the middle update. It then checks:

- The menu is the line's own child, named `epochLineMenu`. It offers the
  four actions in order. Closed, it deletes itself.
- Opened again, the window is destroyed from the event loop while the menu is
  open. The window, the control, the line and the menu all go, and no popup
  is left behind.

The helper runs its step from the event loop once the menu shows, so the
same course also runs against the old nested `exec()` loop.

**Rows (`build/dev`, MSVC 19.51 Debug, Qt 6.11.2, offscreen, serial).** The
records are station logs under `build/logs/bpr-1034-main-win/` and are not
committed.

| Product | `qt_epoch_comparison_scale_100` |
| --- | --- |
| Old stack `exec()` menu (the negative control) | `Exception: SegFault`, 2.22 s. The Debug CRT reports `debug_heap.cpp(904) : Assertion failed: _CrtIsValidHeapPointer(block)` and `(908) … is_block_type_valid`, then the child exits `0xC0000005`. |
| Heap child with `popup()` | Passes. `_100`, `_125` and `_200` pass (2.10–2.47 s), and `_100` passes again 5 of 5 (`--repeat until-fail:5`). |

`clang-format --dry-run --Werror` is clean on both files. macOS and Linux
were not run from this station. The course is the same there, and the
product change is platform-neutral Qt.
