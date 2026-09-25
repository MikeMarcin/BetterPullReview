---
schema: "implement-rfc.card.v1"
id: "BPR-1032"
title: "Qt 6.11 keeps a HarfBuzz buffer in every measured line's layout, about 1.8 KiB a line"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0893, Mac batch 2, 2026-09-23","found_at":"67d5be84","related":["BPR-0893","BPR-0967"],"upstream":"qtbase 8209078e0e (2025-11-04), QTextEngine: keep the hb_buffer_t across shapeTextWithHarfbuzzNG calls"}
acceptance: [{"criterion":"The large review's large_file_ready live heap, which application_large_review_resources prints as LiveHeapGrowthBytes, drops by most of the 70 MiB","outcome":"pass","revision":"27c35729","evidence":{"kind":"command","detail":"Serial runs, same head with only measured_alignment_axis.cpp reverted as the before: large_file_ready LiveHeapGrowthBytes 164.2 MiB before in all 6 resources/loaded_close runs, 101.1 MiB after in all 6: a 63.1 MiB drop of the 70.4 MiB malloc_history attributed. Peak footprint 202.4 -> 138.7 MiB (loaded_close), cache_bytes 177.5 -> 166.7 MiB. Timing unchanged (benchmark 3+3 runs). build/bpr1032-memory/, build/bpr1032-timing/.","path":"docs/issues/60-done/BPR-1032-qt-6-11-keeps-a-harfbuzz-buffer-in-every-measured-lines-layout.md#fixed-unwrapped-panes-measure-in-transient-layouts-mac-main-station-2026-09-23-mac-batch-3"}},{"criterion":"Pane geometry stays exact: qt_syntax_pane_composition, qt_diff_workbench_scale_100/125/200 and the large-review courses pass, with a fallback-font line in a fixture","outcome":"pass","revision":"27c35729","evidence":{"kind":"command","detail":"qt_diff_axis_scale_100/125/200 now assert every boundary of a 400-line NoWrap pane with fallback-font, tab, 24 pt inline and 30 pt additional-format lines equals the pane's native blockBoundingRect sum exactly, with and without a trailing newline; four negative controls fail by name (build/bpr1032-negative-controls.log). Final pass (build/bpr1032-final.log): 8/8 Qt rows incl. qt_syntax_pane_composition and qt_diff_workbench_scale_100/125/200; 7/7 large-review rows (benchmark, unavailable, approximate, cache_bytes, cache_entries, loaded_close, resources). application_large_review_history fails identically without this change on its GUI-stall budget (every epoch refresh ~167 ms vs 100 ms), not on geometry; filed as BPR-1043.","path":"docs/issues/60-done/BPR-1032-qt-6-11-keeps-a-harfbuzz-buffer-in-every-measured-lines-layout.md#fixed-unwrapped-panes-measure-in-transient-layouts-mac-main-station-2026-09-23-mac-batch-3"}}]
---

# BPR-1032: Qt 6.11 keeps a HarfBuzz buffer in every measured line's layout, about 1.8 KiB a line

## Finding

Since the Qt 6.11.2 upgrade (`92c4e159`, 2026-09-21), opening the large
review's 20,000-line comparison holds about 70 MiB more live memory than it
did under Qt 6.10.3. [[BPR-0893]] measured it and traced it to one upstream
change:

- qtbase `8209078e0e` ("QTextEngine: keep the hb_buffer_t across
  shapeTextWithHarfbuzzNG calls", 2025-11-04) turned the HarfBuzz buffer that
  shaping used to create and destroy on every call into a `QTextEngine` member.
  It is released only in `~QTextEngine`. `clearLayout()`, `freeMemory()` and
  `invalidate()` leave it alone. Qt's `6.11` and `dev` branches are unchanged.
- A `QPlainTextEdit` keeps one `QTextLayout` per block for the life of the
  block. The review panes measure every line natively
  (`plain_text_pane_measurement::advance`, `measured_alignment_axis.cpp`),
  and that shapes every block. So every measured line now retains one
  320-byte buffer and two 768-byte arrays: 1,856 bytes.
- Measured with `malloc_history` at `large_file_ready`, same course, the two
  Qt versions: the allocations under `plain_text_pane_measurement::advance`
  are 159,259 for 17.0 MiB at Qt 6.10.3 and 159,176 for 73.5 MiB at Qt
  6.11.2. `QTextEngine::shapeTextWithHarfbuzzNG` alone holds 70.4 MiB in
  119,340 live allocations, three per line, where Qt 6.10.3 held none.
- The commit before the upgrade (`92c4e159^`) and the upgrade itself, whose
  only source change is a Windows header, differ by that amount. The process
  footprint above start at `large_file_ready` is 121.6–122.6 MiB before and
  184.2–186.2 MiB after.

The cost scales with measured lines, not with the file's bytes. A
100,000-line generated file compared side by side would hold about 350 MiB
for this alone. The code is platform-independent, so Windows and Linux pay it
too, although only macOS has measured it.

## Outcome

Either the review panes stop paying a retained HarfBuzz buffer for every line
they measure, or this card records why that costs more than it saves.

## Scope

- The panes are `NoWrap`, but they measure natively because a line that falls
  back to another font can be taller than its neighbours. Any change has to
  keep that exactness. A line whose every character the primary font covers,
  with no format that changes the font, might be measured without shaping it.
  Whether that fast path is exact is a question to settle, not an assumption.
- There is no public Qt API that releases one block's buffer short of
  destroying the block. Reaching into `QTextEngine` through private headers is
  not an answer. Neither is patching the pinned Qt.
- Report upstream if there is a case. The buffer is per engine by design, and
  a `QPlainTextEdit` is the case where one engine per block makes that
  expensive.
- The Mac footprint budgets [[BPR-0893]] re-derived accept this cost at
  224 MiB. A fix here lowers the measured peak, so re-derive them again from
  the new measurement rather than leaving the headroom in place.

## Acceptance

- The large review's `large_file_ready` live heap, which
  `application_large_review_resources` prints as `LiveHeapGrowthBytes` since
  [[BPR-0893]], drops by most of the 70 MiB, or the card records the
  measurement that says it cannot.
- Pane geometry stays exact: `qt_syntax_pane_composition`,
  `qt_diff_workbench_scale_100/125/200` and the large-review courses pass,
  with a fallback-font line in a fixture.

## Boundaries

The core decides, the host performs, Qt presents. Measurement stays in the Qt
adapter. No dependency, pin or Qt patch changes without their own decision.

## Fixed: unwrapped panes measure in transient layouts (Mac main station, 2026-09-23, Mac batch 3)

**The change** is in `src/adapters/qt/src/measured_alignment_axis.cpp`. A
`NoWrap` pane no longer asks its own document layout for every block's
height. Each block is laid out in a `QTextLayout` of its own, bound to the
same block, and destroyed straight after, so the HarfBuzz buffer goes with
it. Binding it to the block means it reads the same document formats and the
same paint device as the pane. It is given the pane's additional formats,
which is where syntax colouring lives, and it repeats the steps
`QPlainTextDocumentLayout::layoutBlock` and `blockBoundingRect` take:
the document's default text option, leading included, the negative-leading
correction, and the bottom margin on the document's last block. Everything
used is public: `QTextLayout(const QTextBlock&)` is declared in the public
`qtextlayout.h`, and it is the constructor `QTextBlock::layout()` itself
uses. No private header is included, and nothing reaches into `QTextEngine`.

Two things still go to the pane's own layout:

- **The widest line so far.** The pane's horizontal range comes only from
  lines it has laid out itself. A block wider than any the pane already knows
  is therefore laid out natively, which leaves the range where measuring every
  block natively left it. Those are record lines, a handful in a file.
- **Wrapping panes and hidden blocks.** There, laying a block out also sets
  its line count, and the pane's own scrolling reads that. The review panes
  are `NoWrap`, so this path is unchanged and keeps the old cost.

Off-screen blocks are left unlaid. The pane shapes them when they scroll into
view, as any `QPlainTextEdit` does. So the retained cost now scales with the
lines a reviewer has looked at, not with the file.

**Exactness** (`measured_alignment_axis_tests.cpp`,
`test_transient_no_wrap_measurement`, run with and without a trailing
newline). A 400-line `NoWrap` pane holds fallback-font lines (`日本語 αβγ 😀`),
tabs, empty lines, a 24 pt inline format, and a 30 pt additional format on
unlaid blocks. Every one of its 401 boundaries equals the pane's native
`blockBoundingRect` sum exactly, at scales 1.0, 1.25 and 2.0. Batched and
synchronous measurement agree, an off-screen block's own layout is left with
no lines, and the horizontal range after measurement equals the range once
every block has been laid out natively. Four negative controls, each a
temporary edit to the measurer, fail by name (`build/bpr1032-negative-controls.log`):

| Removed | Fails at |
|---|---|
| copying the block's additional formats | `boundary 3: measured 49, native 82` |
| natively laying out a new widest line | `the pane's horizontal range already covered its widest line` |
| the last block's bottom margin | `boundary 400: measured 7908, native 7912` |
| the transient path itself (every block native, as before) | `measurement leaves an off-screen block's own layout unshaped` |

**The live heap** (`build/bpr1032-memory/`, M4 Pro, Debug, Qt 6.11.2,
serial runs, before = the same head with only `measured_alignment_axis.cpp`
reverted):

| Sample | Before | After |
|---|---|---|
| `large_file_ready` live-heap growth (resources and loaded_close, 3 + 3 runs each) | 164.2 MiB every run | 101.1 MiB every run |
| `large_file_ready` footprint growth | 183.3–188.7 MiB | 121.4–123.9 MiB |
| highest footprint, resources / loaded_close | 186.6 / 202.4 MiB | 122.8 / 138.7 MiB |
| highest footprint, cache_bytes (2 runs each) | 176.6–177.5 MiB | 165.5–166.7 MiB |
| highest live heap, cache_bytes | 231.0 MiB | 167.7 MiB |
| cache_entries peak footprint | 79.9–81.2 MiB | 81.2–82.7 MiB |
| cleanup live heap, open / destroyed | 65.6–67.6 / 12.9–13.8 MiB | unchanged |

`large_file_ready` falls by 63.1 of the 70.4 MiB `malloc_history`
attributed to the retained buffers. The rest is the blocks that are
laid out natively anyway: the visible ones, the lines the syntax pass
formats, and the record-width lines.

**Timing did not regress** (`build/bpr1032-timing/`, the ordinary benchmark,
3 runs each). `paged_review_and_git_load` took 10.23–10.39 s before and
10.08–10.21 s after. `large_file_selection` took 1.90–1.92 s before and
1.89–1.91 s after, with a maximum GUI gap of 108–109 ms before and
104–108 ms after. Rapid selection took 7.78–7.81 s before and 7.57–7.62 s
after. The measurement's p95 input gap is 4.2 ms both ways.

**Budgets re-derived** (`tests/end_to_end/large_review_benchmark.cpp`). The
single 224 MiB footprint bound from [[BPR-0893]] is replaced by two. The
resources, loaded-close and cache-entry courses return to the original
160 MiB, 15% above their new highest peak of 138.7 MiB. That is the margin
160 MiB had over Qt 6.10.3's 138 MiB. The cache-byte course fills the diff
cache to its byte budget on purpose and exceeded 160 MiB even under
Qt 6.10.3 (168–172). It gets 184 MiB, 10% above its 166.7 MiB peak, which is
BPR-0893's rule. The cleanup live-heap bound tightens from 96 to 84 MiB. The
loaded comparison now holds about 34 MiB live rather than 100, so a cleanup
that kept it would leave 101–110 MiB, and 96 MiB would catch that by only
5 MiB. 84 MiB sits halfway between the observed 67.6 MiB and the retained
101.1 MiB. The destroyed bound, 32 MiB, is unchanged. Negative control: the
before build under the new bounds fails `application_large_review_resources`
with `sampled Mac fixture footprint growth exceeded its budget`
(`build/bpr1032-timing/baseline/resources-new-budget.log`).

**A red row that is not this card's.** `application_large_review_history`
fails at `epoch refresh exceeded the provisional measured Mac budget`, before
and after this change alike. It was run alone, once each, at load 1.8–2.4.
Every one of the 31 epoch refreshes holds the GUI thread for about 167 ms
(median 166.5 ms before and 167.4 ms after; the worst single gaps were
173.0 and 206.8 ms). The input limit is 100 ms, and it was set when the same
course measured 67.9 ms. The file-selection phases, which are where this
change's measurement runs, stay at 30 ms either way. Every refresh completes
in 8.2–8.4 s, inside its 12 s limit. No open card recorded the row, so it is
filed as [[BPR-1043]].

**The final candidate's pass** (`build/bpr1032-final.log`, rebuilt with the
final budgets, ninja then idle). Eight Qt rows at `-j3`, 8/8:
`qt_diff_axis_scale_100/125/200`, `qt_syntax_pane_composition`,
`qt_diff_workbench_scale_100/125/200` and `qt_composed_github_scale_100`.
Seven large-review rows at `-j1`, 7/7: the benchmark, unavailable and
approximate public paths, plus `cache_bytes` (329.99 s), `cache_entries`,
`loaded_close` and `resources` under the new bounds. The earlier integration
pass over the same product change (`build/bpr1032-acceptance.log`, `-j4` at
load 20) also passed `qt_comparison_scroll_controller_*`, `qt_diff_link_map_*`,
`qt_composed_github_scale_125/200`, `qt_workbench_window_*`,
`application_diff_workbench_public_path`,
`application_diff_workbench_shutdown_public_path`,
`application_workbench_composition_contract`, `large_review_fixture_contract`,
`large_review_replay_contract` and `architecture_boundaries`. In that pass,
`cache_bytes` failed the 160 MiB bound (its 165.5–166.7 MiB peak led to its
own 184 MiB bound), and `history` failed the way [[BPR-1043]] records.

`clang-format` 23.1.1 reports the three touched files clean, and
`clang-tidy` reports nothing on a changed line. The files' pre-existing
naming findings, and the `home_view_state.hpp` finding reached through an
include, are untouched.

**Not measured here.** Linux and Windows run `qt_diff_axis_scale_*` in their
sweeps. That test compares the transient answer with each platform's own
native one, so a platform whose shaping differs would fail it by name, but
neither station has run it yet. The large-review memory rows are macOS-only.
The upstream report the scope suggests has not been filed: a report to Qt
needs an account and is an external action, so it is left to the owner.

## Post-close: on Windows the change stops the shared comparison scrollbar short, and `qt_diff_workbench_scale_*` fail (bpr-main-win, 2026-09-24)

**The second criterion does not hold on Windows.** `qt_diff_workbench_scale_100`,
`_125` and `_200` fail there at `3f8f954d`. With only this card's
`measured_alignment_axis.cpp` put back as it was before `27c35729`, they
pass. This station ran `qt_diff_axis_scale_100/125/200` for the first time,
and all three pass. So each block's transient height agrees with the native
one, yet the composed panes do not reach their end. The card stays Done. The
fix needs a follow-up card, whose title has gone to the integrator for an ID.

The runs used `build/dev` (MSVC 19.51 Debug, Qt 6.11.2), offscreen, serially.
The logs are station records under `build/logs/batch7-main-win/`
(`axis-rows-3f8f954d.log`, `trial-revert-1032-*.log`, `probe-*.log`), not
committed.

| Run | Result |
| --- | --- |
| This station's Mac batch 2 sweep, 2026-09-23 at 20:17Z and 20:30Z, an hour before `27c35729` was committed | `qt_diff_workbench_scale_100` passes, in 16.80 s and 16.32 s. |
| CTest at `3f8f954d` | The three rows fail in 2.1–2.6 s at [[BPR-0455]]'s "past a one-sided block the two panes show different numbers on the same row, because neither is renumbered to match the other". `qt_syntax_pane_composition` passes, and so do `qt_diff_axis_scale_100/125/200`. |
| The suite's binary at `3f8f954d`, run directly | It fails the same way. |
| The same, with `measured_alignment_axis.cpp` reverted to `27c35729^` in the working tree | It passes in full: `diff-workbench-tests:pass`. |

**What the panes do.** A temporary probe in that test printed where the
panes stood after `shared->setValue(shared->maximum())`. The probe and the
revert were restored, and the tree was rebuilt at `3f8f954d`.

| | Shared scrollbar | Left pane (112 blocks) | Right pane (116 blocks) |
| --- | --- | --- | --- |
| At `3f8f954d` | `24370/100000` | scroll 8 of 64, top block 8 | scroll 8 of 68, top block 8 |
| With the revert | `100000/100000` | scroll 64 of 64, top block 64 | scroll 68 of 68, top block 68 |

So on Windows, asking the shared scrollbar for its end leaves it about a
quarter of the way down. Both panes stop at the same block, and their
gutters match. Past the first failure the suite stops, so none of its later
rows, [[BPR-1107]]'s white-space checks among them, runs on Windows at
`3f8f954d`.

**Not shown.** The course is offscreen. Whether the real `windows` platform,
whose fonts differ, shows the same stop in the application was not run. That
needs a window on the desktop, and it is left for the follow-up with a
foreground clearance. Why the transient measure moves the shared range on
Windows, and not on macOS or Linux, is not diagnosed here.
