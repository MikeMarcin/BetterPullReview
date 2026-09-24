---
schema: "implement-rfc.card.v1"
id: "BPR-1073"
title: "Windows: qt_workbench_window_scale_100/125/200 read the composed link map and the shared scrollbar a fixed four event passes after publishing, before the panes' time-sliced measurement completes"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1022's integration pass","found_at":"b9991925","related_cards":["BPR-1033","BPR-1022","BPR-0940"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1072--BPR-1074--bd67ade43931.json"}
acceptance: [{"criterion":"AC1 Every read in the window row that depends on the panes' time-sliced measurement waits, bounded, for the condition it depends on, and the condition is named in the test","outcome":"pass","revision":"d87e70f0","evidence":{"kind":"command","detail":"A temporary trace at 9f3b09f9 showed that after publish_fixture's four passes the workbench had pending geometry, 4 measurement batches, 0 links, the shared scrollbar disabled and no axis; 37 ms later pending cleared with 5 batches, 5 links, the scrollbar enabled and qmax 1610. settle_geometry waits until geometry_pending() clears (bounded 30 s, the stage named on failure) and then drains; publish_fixture calls it (failure names the publishing line), and the view menu course after hiding and showing line numbers and the ribbon course after its resize call it again. The sized publications' own 10 s wait, which the 60,000-line publication overran at 2.0 (12,566 ms), moved into the helper.","path":"docs/issues/60-done/BPR-1073-windows-qt-workbench-window-scale-rows-read-the-link-map-and-shared-scrollbar-before-the-panes-finish-measuring.md#measured-then-fixed-in-the-test-bpr-main-win-2026-09-24"}},{"criterion":"AC2 qt_workbench_window_scale_100, _125 and _200 pass on the Windows station; if one then stops at a later, unrelated failure, that failure is named and carded","outcome":"pass","revision":"d87e70f0","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug, Qt 6.11.2, contained as registered, serial: qt_workbench_window_scale_100/125/200 pass in 115.59/95.07/100.70 s on the committed content, and all 17 rows of the window-test executable pass (shared snapshot navigation, focus order, title row, strip menu, home discovery, conversation reveal, overview, native window geometry and the three window rows). clang-format clean; clang-tidy reports only the older wait_for_frame finding (4bbecb48).","path":"docs/issues/60-done/BPR-1073-windows-qt-workbench-window-scale-rows-read-the-link-map-and-shared-scrollbar-before-the-panes-finish-measuring.md#measured-then-fixed-in-the-test-bpr-main-win-2026-09-24"}},{"criterion":"AC3 Negative control: with the waits replaced by the fixed four passes, the row fails again on Windows at a read this card names","outcome":"pass","revision":"d87e70f0","evidence":{"kind":"command","detail":"settle_geometry emptied, so only each caller's fixed four passes remain: qt_workbench_window_scale_100 fails in 8.79 s at 'the composed link map paints ribbons to begin with'. With the wait replaced by four more passes instead, it passes (111.81 s): eight passes happen to suffice on this station today, which is why the condition, not a count, is waited for.","path":"docs/issues/60-done/BPR-1073-windows-qt-workbench-window-scale-rows-read-the-link-map-and-shared-scrollbar-before-the-panes-finish-measuring.md#measured-then-fixed-in-the-test-bpr-main-win-2026-09-24"}}]
---

# BPR-1073: Windows: qt_workbench_window_scale_100/125/200 read the composed link map and the shared scrollbar a fixed four event passes after publishing, before the panes' time-sliced measurement completes

## Defect

`qt_workbench_window_scale_{100,125,200}` fail on the Windows station at
`the composed link map paints ribbons to begin with`. They failed the same way
at `b9991925` without [[BPR-1022]]'s change, so the failure is not that
card's.

[[BPR-1033]] found the cause while fixing the same class of failure in the
UIforETW capture. The composed panes measure their lines in batches of about
4 ms, and the link map gets its geometry, and with it its links, only when
that measurement completes. The window test publishes the fixture and then
runs a fixed four event passes (`process_events()` in
`tests/adapters_qt/workbench_window_tests.cpp`) before it reads the link map.
On this station four passes are not enough.

## Measured (bpr-main-win, 2026-09-23, recorded on BPR-1033)

A reverted probe in `test_the_view_menu_reaches_the_composed_source_panes`
printed, after the four passes:

```text
TRACE_LINK_MAP entries=0 rebuilds=0 frames=0 rejected=0 candidates=0 visible=0 frame=0 size=56x677 shown=1
TRACE_WORKBENCH publications=1 failures=0 invalidations=0 geometry=0 geometry_failures=0 measurements=2 batches=4 restarts=0 max_batch_lines=59
TRACE_LATER geometry=1 geometry_failures=0 batches=5 restarts=0 links=5
```

After four passes the workbench had run four measurement batches and had no
geometry; one batch later it had its geometry and five links. A trial wait
for the measurement, the one BPR-1033 used (`settle_geometry`), got past that
line and stopped at the next read,
`the one visible vertical scrollbar is the shared comparison scrollbar (published)`,
in `test_composed_ribbons_follow_the_group_model_through_scroll_and_resize`.
So at least one more read depends on something that settles after the
measurement.

## Scope

- Make each read that depends on the panes' measurement wait for the real
  completion condition, bounded, instead of a fixed count of passes, as
  BPR-1033 did for the capture. Name the condition each read waits for,
  including the shared scrollbar's.
- Change only the test. If a wait shows a product defect rather than a slow
  settle, record it and card it.
- Where the measurement completes within four passes, as it evidently does on
  macOS and Linux, the waits return at once.

## Acceptance

- AC1 Every read in the window row that depends on the panes' time-sliced
  measurement waits, bounded, for the condition it depends on, and the
  condition is named in the test.
- AC2 `qt_workbench_window_scale_100`, `_125` and `_200` pass on the Windows
  station. If one then stops at a later, unrelated failure, that failure is
  named and carded.
- AC3 Negative control: with the waits replaced by the fixed four passes, the
  row fails again on Windows at a read this card names.

## Evidence

### Measured, then fixed in the test (bpr-main-win, 2026-09-24)

`build/dev`, MSVC 19.51 Debug, Qt 6.11.2, offscreen. Logs are station
records under `build/logs/bpr-1073-main-win/` and are not committed.

**Before.** At `9f3b09f9`, `qt_workbench_window_scale_100` failed in 9.10 s at
`the composed link map paints ribbons to begin with`. A temporary trace, not
committed, read the surface after `publish_fixture`'s four passes and then on
every pass for 3 s:

```text
t+0   pending=1 batches=4 links=0 shared_enabled=0 qmax=0
t+37  pending=0 batches=5 links=5 shared_enabled=1 qmax=1610
```

The link map's links, the scroll controller's axis, and so the shared
scrollbar's enabled state, all arrive together, when the measurement
completes. The controller disables the scrollbar while its axis is
suspended for the measurement (`suspend_axis`), so the shared-scrollbar read
depends on the same completion. One condition covers all three:
`geometry_pending()` clears. This run did not reproduce why BPR-1033's trial
stopped at the shared-scrollbar read. That trial waited only in two courses;
here every publication waits.

**The change** is in `tests/adapters_qt/workbench_window_tests.cpp` only.

- `settle_geometry(surface, stage)` waits until `geometry_pending()` clears,
  bounded at 30 s, then drains as before. Its failure names the stage.
- `publish_fixture` calls it. Every caller then reads a measured surface, and
  a failure names the line of the publication that did not finish.
- The view menu course calls it again after hiding and after showing the line
  numbers, and the ribbons course after its resize. Each of those changes the
  panes' viewports, so they measure again.

**A second limit behind the first.** With a 5 s bound, the row got past both
reads and then failed at `published at line 4977`, the sized publications.
The 60,000-line Debug publication measures for longer than that. Its own wait
was bounded at 10 s from the publication, and on this station it takes, with
publication and measurement together:

| Scale | 60 / 6,000 / 60,000 lines (`publish-ms`) |
| --- | --- |
| 1.0 | 74 / 937 / 9,186 |
| 1.25 | 82 / 957 / 9,246 |
| 2.0 | 143 / 1,364 / 12,566 |

So at 2.0 that course would also have failed on Windows, once the rows
reached it. The bound now lives in `settle_geometry`, at 30 s. The sized course
no longer keeps its own wait, and its `publish-ms` still includes the
measurement, as before.

**Negative controls (AC3).** Each was a temporary edit to `settle_geometry`,
rebuilt and run as `qt_workbench_window_scale_100`, then restored.

| `settle_geometry` | Result |
| --- | --- |
| No wait, so only each caller's fixed four passes remain | fails in 8.79 s at `the composed link map paints ribbons to begin with` |
| The wait replaced by four more passes | passes, 111.81 s |

The second control shows why a pass count is the wrong condition. Eight
passes are enough on this station today, as four were on macOS and Linux, and
that was so for the four before it too.

**Rows (AC2).** `qt_workbench_window_scale_100`, `_125` and `_200` pass,
115.59, 95.07 and 100.70 s, on the committed content. They passed before it
as well: 108.28, 112.59 and 122.00 s, with a 30 s bound but before the
sized course's wait moved. Every row of the same executable passes, 17 of 17:
`qt_shared_snapshot_navigation`, `qt_workbench_focus_order_scale_*`,
`qt_workbench_title_row_scale_*`, `qt_workbench_strip_menu`,
`qt_workbench_home_discovery_scale_*`, `qt_workbench_conversation_reveal`,
`qt_workbench_overview`, `qt_native_window_geometry` and the three window
rows. The full window row now reaches [[BPR-1074]]'s drawer courses on
Windows, and they pass there too. `clang-format` reports the file clean.
`clang-tidy` reports only the older `wait_for_frame` finding (`4bbecb48`).

**macOS and Linux.** The waits return at once where the measurement has
already finished, and there the rows already passed. Wanted as a post-close
confirmation on bpr-main-mac and bpr-main-linux:
`ctest --preset dev -R '^qt_workbench_window_scale_(100|125|200)$' --output-on-failure`.
