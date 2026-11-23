---
schema: "implement-rfc.card.v1"
id: "BPR-2317"
title: "One long text block's layout holds the GUI thread past the input target while a large review loads"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2317--BPR-2317--bbb091e0a705.json","origin":"BPR-2215's WSL2 i9 profile of the load-phase input gap; the owner waived the miss for 0.5.0 and asked for this card (2026-11-20)","related_cards":["BPR-2215","BPR-1516"]}
---

# BPR-2317: One long text block's layout holds the GUI thread past the input target while a large review loads

## Finding

Measured on the WSL2 i9 (Release, `-j8`, PC otherwise idle) at `82ed12f26`,
in `application_large_review_responsiveness`:

- Without a profiler, `paged_review_and_git_load_input_gap` exceeded the
  100 ms input target in 9 of 30 runs with the data root on disk (104 to
  134 ms), mostly the GUI thread's own CPU time.
- Under `perf` (cpu-clock at 1 kHz, DWARF stacks), six runs: the phase's
  gap was 106 to 135 ms, 90 to 113 ms of it the GUI thread's CPU. In each run
  the phase's longest main-thread stretches are single posted events of 112
  to 123 ms, twice per phase, all in
  `diff_workbench::rebuild_geometry` → `plain_text_pane_measurement::advance`
  → `pane_block_measurer::height`: Qt text layout and HarfBuzz shaping
  (`QTextLine::layout_helper`, `QTextEngine::shapeText`).

`rebuild_geometry` (`src/adapters/qt/src/diff_workbench_marks_and_navigation.cpp`)
measures in groups of 16 lines and checks its 4 ms slice only between groups.
A native block is measured whole ("An individual native block is
indivisible"), so one long block in the large-review fixture's diff holds the
thread for its whole layout. Not looked at on macOS or Windows.

## 0.5.2 follow-up

The optimized build 85 binary from stable 0.5.1 (`bf6e5a70a`) reproduced both
waived cases on the designated WSL2 station. The serial, disk-backed run
measured the `paged_review_and_git_load` input gap at 106.481 ms in
`application_large_review_responsiveness` and 112.671 ms in
`application_large_review_launch`, against the 100 ms target. The GUI thread
used 90.725 ms and 96.056 ms of CPU respectively, with zero measured runnable
wait in either gap. At the 0.5.2 merge baseline (`2f1171262`), the application
and test source trees are identical to build 85. The 0.5.1 decision applied
only to that release.

Build 79 probes of this same phase found no overlapping geometry callback
above the 8 ms trace threshold. The worst interval spanned an accepted-read
notification and the following native render. Samples included accepted
snapshot projection and conversation relayout. Trials that coalesced
Resolve availability updates, skipped closed-card relayout, or reduced
timeline summary work each still exceeded the target. These observations
leave exact build 85 attribution open. Keep the separately observed
single-block layout problem in the acceptance criteria.

Reproduce the gap on the 0.5.2 candidate and correlate notification and
main-thread stacks within that same interval. Measure accepted Overview and
conversation publication separately from the benchmark's required native
render. Retain the 100 ms target and the provisional and accepted rendering
contracts when testing reductions to the work on that path.

## Direction (not decided)

Bound one block's measurement: for example, estimate a very long block's
height without a full layout and refine it off the input path, or lay it out
in pieces across turns. The measured heights must stay exact once settled
(BPR-1516's alignment).

## Acceptance

- AC1. On the WSL2 i9, Release, `paged_review_and_git_load_input_gap` meets
  100 ms in 30 of 30 runs with the data root on disk.
- AC2. No single geometry-measurement event on the GUI thread runs past the
  input target for one long block, shown by a course with a block long
  enough to take more than 100 ms to lay out whole.
- AC3. Pane heights and alignment after measurement settles are unchanged.
