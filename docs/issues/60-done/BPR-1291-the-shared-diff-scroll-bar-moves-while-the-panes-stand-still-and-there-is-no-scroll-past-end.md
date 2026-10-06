---
schema: "implement-rfc.card.v1"
id: "BPR-1291"
title: "The shared diff scroll bar moves while the panes stand still, and the panes cannot scroll past their end"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1291--BPR-1291--15e229b6f060.json","related_cards":["BPR-0208","BPR-1264","BPR-1286"],"owner_references":["docs/design/owner-refs/BPR-1291-owner-ref-bpr-read-scroll.webp","docs/design/owner-refs/BPR-1291-owner-ref-vscode-before-scroll.webp","docs/design/owner-refs/BPR-1291-owner-ref-vscode-after-scroll.webp"],"acceptance":["BPR-1291:cause","BPR-1291:bar-agrees","BPR-1291:past-end","BPR-1291:aligned","BPR-1291:kept","BPR-1291:large-review"]}
acceptance: [{"criterion":"BPR-1291:cause","outcome":"pass","revision":"1aaf8f781","evidence":{"kind":"freeform","detail":"The card's Cause section: the shared position is the rail's crossing and ranged over the whole comparison while each pane was clamped to [0, document - viewport], so the bar moved with every pane still for the first 43 % of a viewport and the last 57 % of travel, all of it for a file shorter than its viewport; QPlainTextEdit stopped with its last line at the bottom."}},{"criterion":"BPR-1291:bar-agrees","outcome":"pass","revision":"1aaf8f781","evidence":{"kind":"command","detail":"qt_diff_workbench_scale_{100,125,200}: test_the_panes_scroll_past_their_end_and_the_bar_agrees on the production workbench with renderer.cpp (7 vs 14 lines): bar min = both first lines at top, bar max = lines 8 and 15 at top, every one-line bar step moves a pane and none moves back; 160-line file in fiftieths. Negative control: right pane moved one line behind the bar is caught (ribbons off, ends not level). Old product (base src/adapters) fails 'the bar's maximum puts each side's last line at the top'. qt_comparison_scroll_controller_scale_{100,125,200} pin the exact range, quarter-pixel dead-travel sampling by pane origin, and no travel for one line a side."}},{"criterion":"BPR-1291:past-end","outcome":"pass","revision":"1aaf8f781","evidence":{"kind":"command","detail":"Same rows: wheel and End take the short file to its last line at the top and no further, Home/End on the 160-line file, one-pane bar max = last line. application_diff_workbench_public_path (real composition, full-window capture 19-comparison-end) requires Home = first lines and End = last lines at the top; passes."}},{"criterion":"BPR-1291:aligned","outcome":"pass","revision":"1aaf8f781","evidence":{"kind":"command","detail":"At every step of both workbench sweeps every visible ribbon meets both panes exactly at the lines it names, and once both panes move the two files' last lines are level; controller test checks equal end offsets across the bar."}},{"criterion":"BPR-1291:kept","outcome":"pass","revision":"1aaf8f781","evidence":{"kind":"command","detail":"macOS build/dev Debug: 126/126 of 121 qt_ rows, application_diff_workbench_public_path, active_changed_scaling, profile_multi_window, rate_limit_render_recovery, architecture_boundaries. application_profile_window_review_restoration_public_path fails BPR-1236's check with and without the change (2/2 on base src/adapters). After the rebase: 13/13 focused rows (diff workbench, controller, window, link map, syntax, diff public path, architecture_boundaries)."}},{"criterion":"BPR-1291:large-review","outcome":"pass","revision":"1aaf8f781","evidence":{"kind":"command","detail":"Optimized tests-on macOS tree (app-release, BUILD_TESTING=ON, -O3): 18 large-review rows 18/18 before (142.2 s) and after (134.8 s) the rebase onto 8e6c0a042; all 14 reports BudgetsEnforced true, KnownInputMisses empty, no input failure. After the rebase, largest gaps ms: large_file_selection 12.3-18.4, large_source_revisit_render 8.5/14.6, approximate_file_selection 15.4, first_tab_file 19.6, largest any phase 37.4 (first_tab_head). cache_bytes footprint is near its budget on this machine regardless: in isolation 2/6 candidate and 4/6 base runs failed it."}}]
---

# BPR-1291: The shared diff scroll bar moves while the panes stand still, and the panes cannot scroll past their end

## Origin

The owner, in chat on 2026-10-06, verbatim:

> It's weird that the main diff pane has a scroll bar that moves when the
> content doesnt scroll I would expect it to work like vscode where you can
> scroll until only the last line is visible

Their screenshots (fixture and repository content only):

- [`BPR-1291-owner-ref-bpr-read-scroll.webp`](../../design/owner-refs/BPR-1291-owner-ref-bpr-read-scroll.webp):
  Read on `renderer.cpp` from the `nested-folders-v1` fixture, 8 lines on the
  left and 15 on the right. The shared scroll bar right of the right pane has
  a thumb that moves, but neither pane scrolls.
- [`BPR-1291-owner-ref-vscode-before-scroll.webp`](../../design/owner-refs/BPR-1291-owner-ref-vscode-before-scroll.webp)
  and [`BPR-1291-owner-ref-vscode-after-scroll.webp`](../../design/owner-refs/BPR-1291-owner-ref-vscode-after-scroll.webp):
  VS Code on a 5-line file before and after scrolling. With scroll-past-end
  it scrolls until line 5, the last, is the only line left, at the top.

## Outcome

- The shared bar has range only where some pane moves. Its minimum shows
  both files from their first line, its maximum shows each file's last line
  at the top, and every step between moves at least one pane.
- Both panes scroll past their end the way VS Code does: a short file, even
  one shorter than the viewport, scrolls until its last line is at the top.
- The two sides stay aligned through the connector: the ribbons keep joining
  the same lines on both sides at every position, past the end included.
- Unchanged: BPR-1264's staged publication, BPR-1286's `settle_geometry`,
  change navigation, the placement of a revealed change on the
  synchronization rail, and the 100 ms input targets.

## Cause

The shared position is where the synchronization rail, 43 % down the
viewport, crosses the comparison. Its range was the whole comparison, from
0 to its extent. Each pane's top is its projection less the rail, clamped
between 0 and the document's height less the viewport's.

- For the first 43 % of a viewport and the last 57 % of the travel, the rail
  moved with every pane clamped. The bar moved and nothing scrolled.
- A file shorter than its viewport is clamped between 0 and 0, so all of its
  travel was like that. That is the owner's `renderer.cpp`: the bar showed a
  half-length thumb that moved, and neither pane could move.
- `QPlainTextEdit` stops with its last line at the bottom of the viewport, so
  no pane could scroll past its end.

## Change

- **Range.** `comparison_scroll_controller` keeps the position only where
  some pane moves: from where the first pane leaves its top to where the last
  pane has its last line at the top. Home, End, the bar's ends, the wheel and
  page keys all stop there. A side with one line never moves, so it sets
  neither end; a comparison of one line a side has no travel and the bar is
  disabled.
- **Past the end.** Past the comparison's end the projection carries on one
  to one on both sides, so the two files' ends stay level and the ribbons
  keep meeting their lines. A pane stops at
  `measured_alignment_axis::last_line_origin`: the top of its last line,
  which is Qt's empty block after a final line break when the pane shows one
  (VS Code counts that line too). The measurement records whether that block
  is there.
- **Panes.** The two panes and the one-pane view scroll on centre
  (`setCenterOnScroll`), which is what lets Qt's bar reach the last line. The
  frame puts the line nearest its origin at the top
  (`top_line_for_origin`) and reads the origin back off the same per-line
  measurement as before ([[BPR-0208]]), instead of mapping a fraction of the
  native bar.
- **Ends kept through a remeasurement.** A remeasurement (a resize, a font
  change) keeps the line on the rail where it is, as before, except at an
  end: a comparison shown from its top, or with its last lines at the top,
  stays so. Without this a window that shrank would scroll a file shown from
  its top, because the range now starts where the rail first moves a pane.
- **A line at the top.** A presentation flip that keeps its top line now asks
  the controller for that pane's top (`reveal_pane_origin`), which also works
  past the end, instead of re-deriving the rail in the workbench.
- **Kept.** Revealing a change still puts it on the rail. Because the panes
  now have room past their end, a change in a file shorter than the viewport
  is centred there too, as VS Code centres a change it goes to; before, such a
  file could not move. [[BPR-1264]]'s staged publication and [[BPR-1286]]'s
  `settle_geometry` are untouched.

## Acceptance

- **BPR-1291:cause.** The card records why the bar had range while the panes
  did not scroll.
- **BPR-1291:bar-agrees.** A Qt test on the production workbench shows that
  the bar's minimum and maximum are the panes' first and last positions and
  that no bar step leaves both panes still. A negative control shows that the
  check sees a pane moved behind the bar's back.
- **BPR-1291:past-end.** A Qt test shows a file shorter than the viewport
  scrolling until its last line is at the top, through the bar, the wheel
  and End.
- **BPR-1291:aligned.** A Qt test shows the connector's ribbons joining the
  panes' own line positions on both sides at every sampled position,
  including past the end.
- **BPR-1291:kept.** The diff workbench, scroll controller, link map,
  workbench window and syntax composition rows pass on macOS `build/dev`.
- **BPR-1291:large-review.** The 18 large-review rows pass with budgets
  enforced in an optimized tests-on macOS tree.

## Result (2026-10-06, macOS 26.6.2 arm64, M4 Pro, Qt 6.11.2)

Candidate `1aaf8f781` (product at `1cff20091`), on `main` at `8e6c0a042`.
The Qt and controller rows and the 18 large-review rows were run before the
rebase (off `226a44882`) and again after it; the rebase brought no change to
a file this card touches but the benchmark's Windows constants.

- **Cause.** Recorded above.
- **Bar agrees.** `qt_diff_workbench_scale_{100,125,200}` run
  `test_the_panes_scroll_past_their_end_and_the_bar_agrees` on the
  production workbench with the owner's `renderer.cpp` (7 lines against 14)
  in a 1260 × 820 workbench. The bar's minimum shows both first lines at the
  top; its maximum shows line 8 and line 15, each file's last line, at the
  top. Stepping the bar one line at a time moves a pane at every step and
  never moves one back. A 160-line file passes the same checks in fiftieths
  of the bar.
  **Negative control:** the right pane moved one line behind the bar's back
  is seen by the same checks, as ribbons off their lines and ends out of
  level. **Control on the old product:** the test against the base's
  `src/adapters` fails at "the bar's maximum puts each side's last line at
  the top", after "a file shorter than the viewport still has travel"
  passed, which is the owner's report.
  `qt_comparison_scroll_controller_scale_{100,125,200}` pin the range
  exactly (rail to last line plus rail), sample every quarter pixel for dead
  travel by pane origin, not projection, and show a one-line-a-side
  comparison with no travel.
- **Past the end.** The same rows: the wheel and End take the short file to
  its last line at the top and no further; End and Home do the same for the
  160-line file. The one-pane view's bar reaches its last line too.
  `application_diff_workbench_public_path` (real composition, Git/cache actor,
  full-window capture `19-comparison-end`) now fails unless Home shows both
  first lines and End both last lines at the top; it passes.
- **Aligned.** At every step of both sweeps every visible ribbon meets both
  panes exactly at the lines it names, and once both panes move the two
  files' last lines are level.
- **Kept.** `build/dev` (Debug): 121 `qt_` rows, the diff public path, the
  scaling, multi-window and rate-limit render rows and
  `architecture_boundaries` pass (126/126). After the rebase the diff
  workbench, controller, window, link map and syntax rows, the diff public
  path and `architecture_boundaries` pass again (13/13). Three test changes follow the new range:
  the white-space captures and the syntax bound go back to the top or a page
  from the end, which the old maximum used to show, and the window course's
  scrolled stage steps a page back from the maximum.
  `application_profile_window_review_restoration_public_path` fails at
  [[BPR-1236]]'s "failed reading restoration preserves exact recorded
  identities and sibling" with and without this change (2 of 2 runs on the
  base's `src/adapters`), so it is not this card's.
- **Large review.** Optimized tests-on tree (`app-release`,
  `BUILD_TESTING=ON`, Apple clang `-O3`): the 18 large-review rows pass 18/18
  in 142.2 s before the rebase and 134.8 s after it. All 14 reports have
  `BudgetsEnforced` true, `KnownInputMisses` empty and no input failure in
  both runs. Largest gaps in ms after the rebase (before it in brackets):
  `large_file_selection` 12.3–18.4 (13.1–22.3), `large_source_revisit_render`
  8.5 and 14.6 (12.1 and 12.6), `approximate_file_selection` 15.4 (23.4),
  `first_tab_file` 19.6 (12.0); the largest gap in any phase is 37.4,
  `first_tab_head` (38.5, `first_tab_base`). The wheel course's position check
  now counts its 1,000 pixels from the bar's start; before that fix six rows
  failed "scroll gesture work/position contract".
  `application_large_review_cache_bytes` is close to its 184 MiB footprint
  budget on this machine today with or without the change: 2 of 6 runs
  failed on the candidate and 4 of 6 on the base's `src/adapters`, with the
  recorded peaks at 175.9–181.8 MiB for both.
- **Checks.** clang-format is clean on every changed file. Strict clang-tidy
  is clean on every changed translation unit but `large_review_benchmark.cpp`,
  whose 23 findings are all on lines this card does not touch.
  `DOCUMENTATION_OK`.
- **Owed.** Linux and Windows station runs of the same rows. The change is
  portable Qt and C++.

**Note: Windows run at `4d2694a54` (holds `68c1fe9b7`; station `bpr-main-win`,
MSVC 14.51, Qt 6.11.2; no remote mutation).**

- **`windows-dev`, Debug, whole tree.** It built with no `warning C` or
  `warning LNK` line. Run with `-j1`, 17 of 20 pass:
  - the scroll controller, link map and diff workbench rows at 100, 125 and
    200 %;
  - the four diff axis scales;
  - `qt_syntax_pane_composition`, `qt_workbench_conversation_reveal` and
    `application_diff_workbench_public_path`;
  - `architecture_boundaries`.

  The three `qt_workbench_window_scale_{100,125,200}` rows fail at "no
  review-bar control folds into an overflow menu at any width". That is
  parked [[BPR-1255]]'s failure, already recorded for Windows, not this
  card's.
- **Optimized tests-on tree** (`build/bpr0967-release`, MSVC `/O2`). It
  built with no warning, and the 18 large-review rows pass 18 of 18 with
  `-j1` in 518.3 s. All 14 reports have `BudgetsEnforced` true, an empty
  `KnownInputMisses` and no limit miss. The largest gap in a gated course is
  59.0 ms (`paged_review_and_git_load`); `approximate_file_selection` is
  23.4 ms and `large_file_selection` 28.3 ms.

**Note: Linux run at `68c1fe9b7` (station `bpr-main-linux`, WSL2, GCC 15.2.0,
offscreen).**

- **Dev tree** (rebuilt whole, no warnings), `-j1`: 17 of 20 pass. Passing:
  `qt_comparison_scroll_controller_scale_{100,125,200}`,
  `qt_diff_workbench_scale_{100,125,200}`,
  `qt_diff_link_map_scale_{100,125,200}`,
  `qt_diff_axis_scale_{100,125,150,200}`, `qt_syntax_pane_composition`,
  `qt_workbench_conversation_reveal`,
  `application_diff_workbench_public_path` and `architecture_boundaries`.
  `qt_workbench_window_scale_{100,125,200}` stop at the known [[BPR-1255]]
  check, as expected. With only that one assertion bypassed, locally and not
  committed, all three pass (24.4, 26.1 and 28.9 s). The test was restored and
  rebuilt.
- **Optimized tests-on tree** (`app-release`, `BUILD_TESTING=ON`, GCC
  `-O3`, `-j1`, idle): the 18 large-review rows pass 18/18 in 99.7 s. Every
  report has `BudgetsEnforced` true, `KnownInputMisses` empty and no input
  failure. Largest gaps in ms: `large_file_selection` 23.1–29.7,
  `large_source_revisit_render` 20.6 and 21.1, `approximate_file_selection`
  40.5, `first_tab_file` 19.1. The largest gated gap is 49.6
  (`first_tab_head`).

`build/app-release-tests` was removed. Done is not reopened.
