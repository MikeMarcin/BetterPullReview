---
schema: "implement-rfc.card.v1"
id: "BPR-2202"
title: "On Windows, a right-click inside a dragged selection names its lines with Segoe UI 9 pt"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-2200's Windows Qt run with the fonts loaded, Windows station over SSH, 2026-11-15","reservation":"docs/issues/_reservations/BPR-2201--BPR-2203--f448700c62dd.json","related_cards":["BPR-2200"]}
---

# BPR-2202: On Windows, a right-click inside a dragged selection names its lines with Segoe UI 9 pt

## Found

BPR-2200 made every Windows Qt course run offscreen with the Windows fonts
and Segoe UI 9 pt. With them, `qt_workbench_line_draft` (the window course
`--line-draft`) newly fails. With no fonts it passes. Lane
`claude/v0-5-0-prerelease-targets-52265d` at `18a70bb96`, Release:

    FAILED: a right-click inside a dragged selection names its lines

This is course (2) of the line-draft case in
`tests/adapters_qt/workbench_window_tests.cpp`: a plain drag across lines 3
to 5, then a right-click on line 4. A station-only diagnostic showed the
menu's caption as `Line 4` and its action as `Add comment`, so the
right-click saw no range. Course (1) just before it, a drag across lines 4
to 6 that the host then restores away from, passes.

Undecided: the drag in course (2) may not select lines 3 to 5 under Segoe UI
(the test's `point(..., at::end)` lands elsewhere), or the right-click may
not keep the selection. Settle which before fixing.

## Scope and acceptance

- With Segoe UI 9 pt on Windows, a right-click inside a dragged selection
  names its lines, as on macOS. If the fault is the test's pointer
  arithmetic rather than the product, correct the test and record that.
- `qt_workbench_line_draft` passes on the Windows station with the fonts
  loaded, and still passes on macOS and Linux.

## Reproduce on the Windows station

As BPR-2200's card describes. Build
`test_better_pull_review_qt_workbench_window_tests`, then run
`QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.0 better_pull_review_qt_workbench_window_tests.exe --line-draft`.

## Settled: the test, not the product (2026-11-15)

Station-only diagnostics before course (2)'s drag showed the following. The
press point for line 3 (window 934,81) is inside the right pane's viewport
(894,73, 191 by 651), but `childAt` there is `documentTabBar`. The pane
itself starts at y 86. The comparison scrolls by the pixel
(`source_pane::set_within_line_offset`): the viewport sat 13 pixels above
the pane, masked, with line 3 as its top line and only its bottom 3 pixels
showing. The test aimed at the middle of a line it could not see, so the
press went to the document tab strip, nothing was selected, and the
right-click named one line. The product drew, masked and hit-tested
correctly. With Segoe UI's shorter lines, the earlier course left the
comparison scrolled; with wider fonts it did not.

Fix (`9cf4cd705`, test only): each course's `reset` scrolls the comparison
to its top (`scroll_controller()->home()`) before clicking line 21. It then
checks that the line the course points at is in the viewport's visible
region, so a course can no longer aim at a hidden line unnoticed.

## Verification

- Windows station, Release, Segoe UI 9 pt: `qt_workbench_line_draft` passes,
  and so does the whole window course at 100%, 125% and 200%.
- macOS `dev`: `qt_workbench_line_draft` and `qt_workbench_window_scale_*`
  pass.
