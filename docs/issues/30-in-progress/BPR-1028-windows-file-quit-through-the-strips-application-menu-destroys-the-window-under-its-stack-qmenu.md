---
schema: "implement-rfc.card.v1"
id: "BPR-1028"
title: "Windows: File > Quit through the strip's Application menu destroys the window under its stack QMenu (accessible invoke)"
status: "In Progress"
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

## Fixed in presentation, offscreen evidence (bpr-main-win, 2026-09-23)

`workbench_window::open_strip_menu()` is now the one way the strip menu
opens. It creates a heap `QMenu` as a child of the window, named
`workbenchStripMenu`, with `Qt::WA_DeleteOnClose`, fills it from the bar's
visible menus as before, and shows it with `popup()` under the control. The
button's `clicked` signal connects to it directly. No handler of the window's
waits for the menu, so nothing is left on the stack when a Quit or close
destroys the window, and the window deletes the open menu with its other
children. [[BPR-1022]] can call the same function.

### The course, and its negative control

`qt_workbench_strip_menu` is a new row, registered only on Windows because
only the Windows strip has the control. It runs
`test_better_pull_review_qt_workbench_window_tests --strip-menu` offscreen at
1.0 and does two things:

1. It opens the menu and checks that it is the window's child, named
   `workbenchStripMenu`, and offers the bar's visible menus in the bar's
   order. It then closes the menu and checks that the menu has deleted itself.
2. It opens the menu again and presses Quit the way UI Automation does. It
   shows File through the File item's accessible `showMenuAction`, then calls
   Quit's accessible `pressAction`. That triggers the action with both menus
   still open, and the course checks that the strip menu is still visible.
   Quit posts the window's destruction, as the desktop's quit does. The
   course then checks that the window is gone, that the strip menu and File
   went with it, and that no popup is left.

The course opens the menu from a timer that polls for it. That works whether
or not the menu runs its own nested loop, so the same course can run against
the old form.

| Candidate | `qt_workbench_strip_menu` |
| --- | --- |
| Old stack `exec()` menu, the new course (negative control) | **Failed**: `debug_heap.cpp(904) : Assertion failed: _CrtIsValidHeapPointer(block)`, then `is_block_type_valid`, `HEAP CORRUPTION DETECTED`, and the child died with `0xC0000005`. This is the quit probe's assertion, reproduced offscreen |
| `open_strip_menu()` | Passed 5 of 5 (`--repeat until-fail:5`), about 0.8 s each |

At the fix, these also passed on this station: `qt_workbench_title_row_scale_{100,125,200}`,
`qt_workbench_focus_order_scale_{100,125,200}`, `qt_workbench_home_discovery_scale_100`,
`qt_native_window_geometry` and `test_network_isolation_manifest` (the new
row is contained). `format_check.py` (clang-format 22.1.8) passes on the four
touched C++ files. Build: `build/dev`, MSVC 19.51 Debug, Qt 6.11.2.

`application_windows_title_row` used to arm a timer before the press and
relied on `exec()` blocking until the timer closed the menu. It now presses
the control, waits for the popup, reads it and closes it, and also checks
that the closed menu deletes itself. That row runs on the real `windows`
platform, so it waits for a foreground clearance together with AC2.

### Same pattern elsewhere, not changed here

`epoch_line::contextMenuEvent` (`src/adapters/qt/src/epoch_line.cpp`) is a
stack `QMenu` parented to the epoch line and run with `exec()`, on every
platform. None of its own actions destroys the window. A window destroyed
while that menu is open, for example by an accessible Quit in the window's
File menu, would free the stack object the same way. That needs its own card.

### On the real desktop (bpr-main-win, 2026-09-23, cleared by the coordinator)

Run at `2a5580d6`, whose product source is `e08a0409`'s, from a `build/dev`
Debug build (MSVC 19.51, Qt 6.11.2) with executable sha256 `8351faa4…`. JaegAW,
Windows 11 10.0.26200, session 1, ordinary user. Each probe had a fresh
owner-private root. The harness is the station's [[BPR-0980]] observer with a
mouse mode added. It is task-owned and not committed.

| Run, 18:37:04Z to 18:37:46Z | Result |
| --- | --- |
| `application_windows_title_row` on the real `windows` platform, new form | Passed, 1.96 s |
| Quit probe 1: posted click on Application menu, UI Automation expands File and invokes Quit | Owner exited 0. No CRT box, no process left, foreground handed back |
| Quit probe 2 | The same, exit 0 |
| Quit probe 3 | The same, exit 0 |
| Quit with posted mouse clicks on File and Quit | Exit 0, no process left |

Before the fix, the same accessible Quit left the owner showing
`_CrtIsValidHeapPointer(block)` in 4 of 4 probes (`c52f5907`, `6f1b83e3…`).
The records are station logs under `build/logs/bpr-1028-main-win/`
(`title-row-2a5580d6.log`, `quit-probe-fix-{1,2,3}.json`, `quit-probe-mouse.json`).

**macOS and Linux:** the product change and the new row are compiled only
under `_WIN32` / `if(WIN32)`, and `application_windows_title_row` is
Windows-only. Neither platform's source or registration changed.
