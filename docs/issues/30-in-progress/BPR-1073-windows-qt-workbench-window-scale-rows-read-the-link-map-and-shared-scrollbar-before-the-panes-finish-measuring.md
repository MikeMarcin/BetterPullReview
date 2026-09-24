---
schema: "implement-rfc.card.v1"
id: "BPR-1073"
title: "Windows: qt_workbench_window_scale_100/125/200 read the composed link map and the shared scrollbar a fixed four event passes after publishing, before the panes' time-sliced measurement completes"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1022's integration pass","found_at":"b9991925","related_cards":["BPR-1033","BPR-1022","BPR-0940"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1072--BPR-1074--bd67ade43931.json"}
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

Pending.
