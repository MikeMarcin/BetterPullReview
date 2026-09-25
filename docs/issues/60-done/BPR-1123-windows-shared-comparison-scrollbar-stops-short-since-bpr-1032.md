---
schema: "implement-rfc.card.v1"
id: "BPR-1123"
title: "Windows: since BPR-1032 the shared comparison scrollbar stops short of the end, and qt_diff_workbench_scale_100/125/200 fail"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"found by bpr-main-win at 3f8f954d, 2026-09-24 (segment 9); recorded on BPR-1032 post-close","origin_card":"BPR-1032","related_cards":["BPR-1032","BPR-1107","BPR-0455","BPR-0869"],"reservation":"docs/issues/_reservations/BPR-1123--BPR-1124--e9357c87ad44.json"}
acceptance: [{"criterion":"AC1 The card records why the shared bar stops short on Windows and not on macOS or Linux, with the measurement that shows it","outcome":"pass","revision":"cb3bff3f","evidence":{"kind":"observation","detail":"Probe at 2ff7e20c (build/dev MSVC Debug, offscreen): at shared->setValue(maximum) the controller had no axis yet (the 112/116-block measurement still running), so the jump was dropped; the later selection reveal framed q=406 of a 1666 px axis = 24370/100000. Per-block heights are exact (Arial 12 px offscreen, leading -1; qt_diff_axis_scale_* pass). Transient measurement ~115 us/line on every pass vs native ~85 us first pass and near-free restarts over laid blocks; Windows Debug links Qt's debug DLLs (dumpbin /dependents: Qt6Cored/Guid/Widgetsd.dll), macOS/Linux Debug link release Qt; the suite waited four processEvents rounds.","path":"docs/issues/60-done/BPR-1123-windows-shared-comparison-scrollbar-stops-short-since-bpr-1032.md#diagnosed-bpr-main-win-2026-09-25"}},{"criterion":"AC2 qt_diff_workbench_scale_100/125/200, qt_diff_axis_scale_100/125/200 and qt_syntax_pane_composition pass on Windows (build/dev) with no check weakened; the suite asserts the shared bar's end brings both panes to their own last page","outcome":"pass","revision":"cb3bff3f","evidence":{"kind":"command","detail":"At cb3bff3f, build/dev rebuilt, CTest -j1 contained as registered (03:51-04:06Z): qt_diff_workbench_scale_100/125/200 pass (15.90/16.82/17.92 s), qt_diff_axis_scale_100/125/200 pass (1.08/1.10/1.18 s), qt_syntax_pane_composition pass (0.74 s). Run directly the suite prints diff-workbench-tests:pass at 1.0, 1.25 and 2.0. Its checks are unchanged; checks after a full first pass wait for geometry (settle_geometry); the new assertion 'the shared bar's end brings both panes to their own last page' passes.","path":"docs/issues/60-done/BPR-1123-windows-shared-comparison-scrollbar-stops-short-since-bpr-1032.md#verification-ac2-ac4"}},{"criterion":"AC3 Negative controls: each new assertion fails when its property is false","outcome":"pass","revision":"cb3bff3f","evidence":{"kind":"command","detail":"Temporary edits, each restored byte for byte: record never read -> 'a viewport-only change reuses every recorded block'; fonts ignored -> 'after a font change the boundaries are the pane's native ones'; reused width skips the widest-line rule -> 'a reused width still lays the widest line out...'; formats not compared -> 'a block whose formats changed is measured again'; numbering check back on four rounds -> 'the shared bar's end brings both panes to their own last page'; record never read with the divider re-measure waited for -> 'the divider's rebuild reuses every recorded line height'.","path":"docs/issues/60-done/BPR-1123-windows-shared-comparison-scrollbar-stops-short-since-bpr-1032.md#verification-ac2-ac4"}},{"criterion":"AC4 No regression in BPR-1032's integration rows, and the unwrapped panes still keep no shaped layout per measured line","outcome":"pass","revision":"cb3bff3f","evidence":{"kind":"command","detail":"26 of 26 at cb3bff3f (and 26 of 26 before the rebase): qt_composed_github_scale_* (39-44 s), qt_workbench_window_scale_* (92-99 s), qt_comparison_scroll_controller_scale_*, qt_diff_link_map_scale_*, application_diff_workbench_public_path, _shutdown_public_path, application_workbench_composition_contract, large_review_fixture_contract, large_review_replay_contract, architecture_boundaries, architecture_boundary_contract (342.69 s), plus the seven AC2 rows. test_transient_no_wrap_measurement still asserts measurement leaves an off-screen block's own layout unshaped; the record keeps two qreals and a shared QList handle per line, no layout.","path":"docs/issues/60-done/BPR-1123-windows-shared-comparison-scrollbar-stops-short-since-bpr-1032.md#verification-ac2-ac4"}},{"criterion":"AC5 Whether the real windows platform needs a foreground check is decided on the diagnosis; if it does, the card lists the exact run","outcome":"pass","revision":"cb3bff3f","evidence":{"kind":"observation","detail":"Decided: not needed for acceptance (the defect was pacing of a time-sliced measurement against fixed event rounds, made visible by debug Qt; exactness is asserted against each pane's own native answer). Offered as an optional foreground run (measured-axis and workbench suites with QT_QPA_PLATFORM=windows at 150%, about 2 minutes), a DirectWrite cross-check BPR-1032 never had; the command is in the coordinator's foreground list.","path":"docs/issues/60-done/BPR-1123-windows-shared-comparison-scrollbar-stops-short-since-bpr-1032.md#ac5-the-real-windows-platform"}}]
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

## Diagnosed, bpr-main-win, 2026-09-25

**The bar does not stop short of a measured end. The jump to the end is made
before the measured axis exists, and it is dropped.** A temporary probe in
the suite (`build/dev`, MSVC 19.51 Debug, Qt 6.11.2, offscreen, at `2ff7e20c`)
printed every axis build, every frame, every measurement batch, and the test's
own view of the jump:

- At `shared->setValue(shared->maximum())` the controller has no axis for the
  112- and 116-block panes. Their measurement is still under way: in one
  probe run the right pane had 32 of its 116 lines measured.
  `comparison_scroll_controller`'s `valueChanged` handler returns without an
  axis, so the jump goes nowhere.
- The axis arrives afterwards, 1666 px of comparison. Its first frame is the
  selection's reveal at 406 px, and 406 / 1666 is the 24370 / 100000 the
  bar settles at. Both panes stand at block 8.
- With that axis in place the bar does reach its end. Once the numbering check
  waits for geometry, it passes at scales 1.0, 1.25 and 2.0, and so does the
  suite's new assertion that the shared bar's end brings both panes to their
  own last page.

**The per-block measurement is exact on Windows, and font metrics are not the
cause.** The offscreen platform resolves the panes' code font to Arial, 12 px
(9 pt, from `QT_QPA_FONTDIR`'s FreeType faces, as [[BPR-0869]] found), with
line spacing 13, height 14 and leading -1. The transient path's negative
leading step handles that. `qt_diff_axis_scale_100/125/200` compare every
transient boundary with the pane's native one and pass. The axis extents
match the documents.

**Why the axis is late: every measurement shapes every line again.**
[[BPR-1032]] measures each unwrapped block in a transient `QTextLayout` and
destroys it, so nothing keeps the answer. The native path it replaced
laid each block out once, and every later measurement read the cached
layout for free. The workbench measures in 4 ms batches of 16-line groups,
one batch per event turn, and it restarts on every show, resize, style or
DPI event. Measured in the same run:

| | Lines per 4 ms batch | Cost per line |
| --- | --- | --- |
| Transient (`27c35729`), every pass | 48 (5.4-5.8 ms per 48) | about 115 us |
| Native (`27c35729^`), first pass | 48 (4.1-4.4 ms per 48) | about 85 us |
| Native, a restart over laid blocks | 112-194 per batch | almost nothing |

The suite's `process_events()` is four `processEvents()` rounds. The
fixture's 228 lines, with a restart, no longer finish in them.

**Why only Windows.** Windows Debug builds link Qt's debug libraries
(`Qt6Cored.dll`, `Qt6Guid.dll`, `Qt6Widgetsd.dll`, from `dumpbin
/dependents`), so HarfBuzz shaping runs unoptimised. macOS and Linux Debug
builds link release Qt, where the same passes finish inside the four rounds.

**What it costs outside the suite.** A re-measure that cannot move a height
(the detail divider, a window made taller, a style or DPI event, a restart
during loading) re-shaped every line. For the time it takes, the shared bar
ignores input and the link map has no ribbons. On the large review that is
tens of thousands of lines, in every build. A width change makes Qt relayout
the whole document, so it re-shapes every line either way.

## Fixed, bpr-main-win, 2026-09-25 (`cb3bff3f`)

Verified as `63c2b6b5` on `79a96104`. It was rebased onto origin's macOS test-only commits `053abefe` to `feabac34`, none of which these rows build, and pushed as `cb3bff3f`: the same patch (patch-id `82632488`).

**Each source pane keeps its measured block heights**
(`src/adapters/qt/src/measured_alignment_axis.cpp`, `pane_block_heights`).
For each block the record keeps the transient height, the natural width, and
the formats list the block was measured with. The list is implicitly shared,
so comparing it is a pointer check while it is the same list. A measurement
given the record reuses a block when both hold:

- **the pane is as it was.** The same document, revision and block count; the
  same pane and document fonts; the same device pixel ratio, margin and tab
  stop. If any of these differs, the record is emptied first;
- **the block is as it was.** Its own additional formats are the list it was
  measured with. A hidden block is never reused.

A reused width still goes through BPR-1032's widest-line rule. So after a
width change has emptied the pane's own layouts, the record-width lines are
laid out natively again, and the horizontal range comes back exactly as a
fresh measurement would set it. The workbench keeps one record per source
pane (`diff_workbench.hpp`) and empties both wherever it replaces their text,
because a document's revision alone does not identify its content. It counts
reused lines in `diff_workbench_statistics::MeasurementLinesReused`.

Still no shaped layout is kept per line. The record costs about 40 bytes a
line: two `qreal`s and a shared `QList` handle. The formats lists belong to
the blocks' layouts already. For the large review's 40,000 measured lines
that is about 1.6 MB, against BPR-1032's 63 MiB saving.
`docs/architecture/diff-pipeline-and-rendering.md` section 9 says so.

**The suite** (`tests/adapters_qt/diff_workbench_tests.cpp`):

- **Checks that follow a full first pass wait for geometry** with the suite's
  own `settle_geometry`, instead of assuming four event rounds suffice. A
  first pass shapes every line. The publications in the numbering, first
  attribution, ribbon-selection (two), boundary-rule and link-map endpoint
  tests, and the font change in the metrics check. Their assertions are
  unchanged. The divider, DPI and style re-measures stay on four rounds,
  because the record makes them cheap again.
- **The symptom is asserted:** "the shared bar's end brings both panes to
  their own last page".
- **The divider's rebuild reuses every line:** `MeasurementLinesReused` rises
  by the two panes' line counts.

**The unit test** (`measured_alignment_axis_tests.cpp`,
`test_recorded_block_heights`). A 120-line unwrapped pane with fallback-font
lines, 30 pt additional formats and a widest line far below the viewport:

- a taller viewport reuses all 120 blocks, and every boundary stays;
- a wider viewport, which makes Qt relayout the document, reuses all 120.
  The boundaries equal the pane's native ones, and the horizontal range
  already covers the widest line;
- a font change reuses nothing, and lands on the native boundaries at the
  new size;
- a block whose formats a highlighter changed (`setFormats` then
  `markContentsDirty`, which leaves the revision alone) is the one block
  measured again. The others are reused;
- a text edit reuses nothing;
- `clear()` reuses nothing, and gives the same boundaries.

## Verification (AC2-AC4)

JaegAW, Windows 11 25H2, MSVC 19.51, Qt 6.11.2, `build/dev` Debug, offscreen.

**The suite passes on Windows at every scale.** Run directly, the diff
workbench suite prints `diff-workbench-tests:pass` at 1.0, 1.25 and 2.0.

**The committed candidate, `cb3bff3f`**: `build/dev` rebuilt, then
BPR-1032's integration set through CTest, `-j1`, contained as registered,
2026-09-25T03:51Z to 04:06Z. 26 of 26 pass:

| Rows | Result |
| --- | --- |
| `qt_diff_workbench_scale_100` / `_125` / `_200` | pass, 15.90 / 16.82 / 17.92 s |
| `qt_diff_axis_scale_100` / `_125` / `_200` | pass, 1.08 / 1.10 / 1.18 s |
| `qt_syntax_pane_composition` | pass, 0.74 s |
| `qt_comparison_scroll_controller_scale_*`, `qt_diff_link_map_scale_*` | 6 of 6 |
| `qt_composed_github_scale_100` / `_125` / `_200` | pass, 39.25 / 39.96 / 43.68 s |
| `qt_workbench_window_scale_100` / `_125` / `_200` | pass, 92.34 / 97.81 / 98.50 s |
| `application_diff_workbench_public_path`, `_shutdown_public_path`, `application_workbench_composition_contract` | 3 of 3 |
| `large_review_fixture_contract`, `large_review_replay_contract` | 2 of 2 |
| `architecture_boundaries`, `architecture_boundary_contract` | 2 of 2, the latter 342.69 s |

The same 26 passed on the same product change before the rebase onto
`79a96104` (at `2ff7e20c` plus the change), and the suite's white-space
checks ([[BPR-1107]]) run on Windows for the first time since BPR-1032.
`test_transient_no_wrap_measurement` still asserts that measurement leaves an
off-screen block's own layout unshaped, so no shaped layout is kept per line.

**Owed elsewhere.** The large-review memory rows are macOS-only. The record
should add about 1.6 MB of live heap at `large_file_ready`, against
BPR-1032's 101.1 MiB. `application_large_review_resources` there would show
it.

**Negative controls.** Each is a temporary edit, restored afterwards; the file
was byte-identical to the candidate after each.

| Edit | Fails at |
| --- | --- |
| The record never read | "a viewport-only change reuses every recorded block" |
| The record ignores the fonts | "boundary 1 differs from the native one", then "after a font change the boundaries are the pane's native ones" |
| A reused width skips the widest-line rule | "a reused width still lays the widest line out, so the pane's horizontal range already covered it" |
| A block's formats not compared | "boundary 4 differs from the native one", then "a block whose formats changed is measured again" |
| The numbering check back on four rounds | "the shared bar's end brings both panes to their own last page" (the card's symptom) |
| The record never read, in the workbench suite | "detail-divider resize coalesces one attributed viewport-axis rebuild without changing selection" (the pre-existing timing check, first) |
| The same, with the divider's re-measure waited for | "the divider's rebuild reuses every recorded line height" |

**Gates.** clang-format 22.1.8 is clean on the six C++ files. HEAD's versions
were clean, so only lines beside the change moved.
`_coding-standard/scripts/tidy.py` reports nothing on a changed line. The
one finding in `measured_alignment_axis_tests.cpp` (`stale_after`) predates
this change.

## AC5: the real `windows` platform

**Not needed for this card's acceptance.** The defect was pacing: a
time-sliced measurement against a fixed number of event rounds, made visible
by Qt's debug libraries. The record's exactness is asserted against each
pane's own native answer, which is the same code on every platform.

**Offered as an optional foreground run.** It would cross-check something
[[BPR-1032]] never had: transient against native heights under the real
platform's DirectWrite fonts rather than offscreen FreeType. The measured-axis
suite and the workbench suite, run directly with `QT_QPA_PLATFORM=windows` at
the display's 150%, take about 2 minutes of the desktop and open their own
windows. Their commands are with the coordinator's foreground list.
