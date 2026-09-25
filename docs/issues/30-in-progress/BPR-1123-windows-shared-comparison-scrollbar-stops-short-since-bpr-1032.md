---
schema: "implement-rfc.card.v1"
id: "BPR-1123"
title: "Windows: since BPR-1032 the shared comparison scrollbar stops short of the end, and qt_diff_workbench_scale_100/125/200 fail"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"found by bpr-main-win at 3f8f954d, 2026-09-24 (segment 9); recorded on BPR-1032 post-close","origin_card":"BPR-1032","related_cards":["BPR-1032","BPR-1107","BPR-0455","BPR-0869"],"reservation":"docs/issues/_reservations/BPR-1123--BPR-1124--e9357c87ad44.json"}
---
# BPR-1123: Windows: since BPR-1032 the shared comparison scrollbar stops short of the end, and qt_diff_workbench_scale_100/125/200 fail

## Finding

[[BPR-1032]] (`27c35729`) stopped measuring an unwrapped pane's blocks in
their own layouts. Each block is measured in a transient `QTextLayout` that is
then destroyed, so Qt 6.11 keeps no HarfBuzz buffer in it. On Windows its
second criterion does not hold: `qt_diff_workbench_scale_100`, `_125` and
`_200` fail. Observed by bpr-main-win at `3f8f954d` (MSVC 19.51 Debug, Qt
6.11.2, `build/dev`, offscreen, serial). BPR-1032's post-close note has the
runs.

| Run | Result |
| --- | --- |
| This station's Mac batch 2 sweep, 2026-09-23, an hour before `27c35729` | `qt_diff_workbench_scale_100` passes, in 16.80 s and 16.32 s |
| CTest at `3f8f954d` | The three rows fail in 2.1–2.6 s at [[BPR-0455]]'s "past a one-sided block the two panes show different numbers on the same row". `qt_syntax_pane_composition` and `qt_diff_axis_scale_100/125/200` pass |
| The suite's binary, run directly | It fails the same way |
| The same, with only `measured_alignment_axis.cpp` reverted to `27c35729^` | It passes in full: `diff-workbench-tests:pass` |

A temporary probe printed where the panes stood after
`shared->setValue(shared->maximum())`:

| | Shared scrollbar | Left pane (112 blocks) | Right pane (116 blocks) |
| --- | --- | --- | --- |
| At `3f8f954d` | `24370/100000` | scroll 8 of 64, top block 8 | scroll 8 of 68, top block 8 |
| With the revert | `100000/100000` | scroll 64 of 64, top block 64 | scroll 68 of 68, top block 68 |

So on Windows, asking the shared scrollbar for its end leaves it about a
quarter of the way down, with both panes on the same block. The per-block rows
(`qt_diff_axis_scale_*`) pass: each block's transient height equals the
pane's native one. Past the first failure the suite stops, so none of its
later rows runs on Windows, [[BPR-1107]]'s white-space checks among them.

Only Windows diverges. macOS and Linux pass the same rows at the same source.

## Scope

- Diagnose it headless. Compare the per-block measurement with Windows font
  metrics: the offscreen platform's font resolution (compare [[BPR-0869]],
  where "monospace" resolves to Arial offscreen), the line height and
  leading, and `QFontMetrics` against `QTextLayout` heights.
- Fix it with a negative control. BPR-1032's memory saving stays: no fix may
  keep a shaped layout in every measured line again.
- If confirming on the real `windows` platform matters, list it as a
  foreground run for the coordinator. Do not run it without clearance.

## Acceptance

- AC1: The card records why the shared bar stops short on Windows and not on
  macOS or Linux, with the measurement that shows it.
- AC2: `qt_diff_workbench_scale_100/125/200`, `qt_diff_axis_scale_100/125/200`
  and `qt_syntax_pane_composition` pass on Windows (`build/dev`), with no
  check weakened. The suite asserts the symptom itself: the shared bar's end
  brings both panes to their own last page.
- AC3: Negative controls. Each new assertion fails when its property is false.
- AC4: No regression in BPR-1032's integration rows, and the unwrapped panes
  still keep no shaped layout per measured line.
- AC5: Whether the real `windows` platform needs a foreground check is
  decided on the diagnosis. If it does, the card lists the exact run.
