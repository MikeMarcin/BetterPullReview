---
schema: "implement-rfc.card.v1"
id: "BPR-1028"
title: "Windows: File > Quit through the strip's Application menu destroys the window under its stack QMenu (accessible invoke)"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"BPR-0980 native activation course, bpr-main-win quit-probe","found_at":"c52f5907","related_rfcs":["RFC-BPR-0028"],"related_cards":["BPR-0653","BPR-0980","BPR-1022"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1027--BPR-1028--763d93af262e.json"}
---

# BPR-1028: Windows: File > Quit through the strip's Application menu destroys the window under its stack QMenu (accessible invoke)

## Defect

[[BPR-0653]] made the strip the Windows title row. Its first control,
"Application menu", opens the menu bar's visible menus (`c6cac3f0`). The
menu is a `QMenu` on the stack of the button's `clicked` handler, parented to
the window, and it runs a nested event loop with `exec()`
(`src/adapters/qt/src/workbench_window.cpp`, the `StripMenuButton`
connection).

Anything that destroys the window while that loop is running deletes the
stack object as one of the window's children. The Debug CRT then asserts
`_CrtIsValidHeapPointer(block)` (`minkernel\crts\ucrt\src\appcrt\heap\debug_heap.cpp:904`),
and a Release build frees a stack address. Afterwards `exec()` returns into a
handler whose window no longer exists.

Found on the Windows station during [[BPR-0980]]. The quit probe opens the
menu with a posted click, expands File with UI Automation, and invokes Quit
with UI Automation. It failed 4 of 4, the process was left showing the CRT's
"Debug Assertion Failed" box, and the course killed it
(`build/logs/bpr-0980-main-win/quit-probe-4.json` on that station; product
source `c52f5907`, executable `6f1b83e3…`). An accessible invoke runs the
action without closing the menu first, so Quit completes while the menu's
loop is still running. A mouse or keyboard choice hides the menu before the
action runs, which is why an ordinary click has not shown it. The exposure is
therefore every assistive-technology user who quits through the menu, and any
other path that destroys the window while its menu is open, such as an
accessible close of the window or a profile shutdown.

[[BPR-1022]] adds Alt and F10 to open the same menu, and its scope says it
must share this path once it is fixed.

## Scope

- Windows only, in `workbench_window`'s Windows branch (Qt presents). The
  menu must not be a stack object, and opening it must not run a nested event
  loop in a handler that belongs to the window. It is a heap child of the
  window, deleted when it closes, and opened with `popup()`. If the window is
  destroyed first, the menu goes with it and nothing on the stack refers to
  either.
- One named opening path that [[BPR-1022]] can call.
- The menu keeps its contents: the bar's visible menus, read from the bar each
  time, in the bar's order. Its object name, its position under the control
  and its accessible names do not change.
- macOS and Linux do not change. Neither has the control.

## Acceptance

- AC1 An offscreen Windows course opens the strip menu, destroys the window
  while the menu is open, and passes: no CRT report, no popup left behind,
  and the menu destroyed with its window. Run against the stack-`exec()` form,
  the same course fails. `qt_workbench_title_row_scale_{100,125,200}` and
  `qt_workbench_focus_order_scale_{100,125,200}` pass on the Windows station.
- AC2 On the real Windows desktop the [[BPR-0980]] quit probe passes N of N
  (N ≥ 3): Application menu, File, then Quit invoked through UI Automation.
  The process exits 0, no CRT box appears, and no process remains. Choosing
  Quit with the mouse still quits.
- AC3 `application_windows_title_row` passes on the real `windows` platform,
  and it now waits for the menu that `popup()` shows rather than depending on
  `exec()` blocking. macOS and Linux are unchanged.
