---
schema: "implement-rfc.card.v1"
id: "BPR-1043"
title: "Every epoch refresh in the large-review history course holds the GUI thread for about 167 ms against a 100 ms budget"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"bpr-main-mac, Mac batch 3, while verifying BPR-1032","found_at":"f97c08e6","related":["BPR-0967","BPR-1032","BPR-0893"]}
acceptance: [{"criterion":"application_large_review_history passes on macOS, with every refresh's largest GUI gap inside the 100 ms budget","outcome":"pass","revision":"7b5d78be","evidence":{"kind":"command","detail":"At 7b5d78be's source (build/bpr1043-history-final.log, alone): Passed; 31 refreshes' worst gaps median 68.8 ms, max 75.3 ms (before: median 166.5-167.4 ms, failing).","path":"docs/issues/60-done/BPR-1043-every-epoch-refresh-in-the-large-review-history-course-stalls-the-gui-thread.md#cause-commit-and-fix-bpr-main-mac-2026-09-23"}},{"criterion":"The card names the cause and the commit, or the window, where the gap grew from 68 ms","outcome":"pass","revision":"7b5d78be","evidence":{"kind":"observation","detail":"Cause: the conversation panel's two doItemsLayout passes per re-projection of 2,000 posts, each sizeHint reshaping heading/body/facts and querying the style (sample: ~108 ms per refresh in apply_conversation_anchor). Commit: cc038748 (parent 24c9e90c passes, cc038748 fails at median 176.7 ms, both Qt 6.10.3). Qt 6.11.2 excluded: today's tree on Qt 6.10.3 fails at 164.0 ms.","path":"docs/issues/60-done/BPR-1043-every-epoch-refresh-in-the-large-review-history-course-stalls-the-gui-thread.md#cause-commit-and-fix-bpr-main-mac-2026-09-23"}},{"criterion":"A focused check fails on the pre-fix behaviour","outcome":"pass","revision":"7b5d78be","evidence":{"kind":"command","detail":"The history course itself fails before the fix (f97c08e6 Qt 6.11.2: 166.5-167.4 ms; d856371b on Qt 6.10.3: 164.0 ms) and passes after. The new test_row_sizes_follow_text_width_and_font fails by name when width, font or text leave the cache key (build/bpr1043-negative-controls.log).","path":"docs/issues/60-done/BPR-1043-every-epoch-refresh-in-the-large-review-history-course-stalls-the-gui-thread.md#cause-commit-and-fix-bpr-main-mac-2026-09-23"}}]
---

# BPR-1043: Every epoch refresh in the large-review history course holds the GUI thread for about 167 ms against a 100 ms budget

## Defect

`application_large_review_history` is red on macOS. It fails at
`large-review-benchmark-failed: epoch refresh exceeded the provisional
measured Mac budget` (`tests/end_to_end/large_review_benchmark.cpp`, the
check on `HistoryInputIntervalLimitMs` and `HistoryRefreshLimitMs`).

It was measured on an M4 Pro, Debug, Qt 6.11.2, with the row run alone at a
load average of 1.8–2.4. Two runs were made, one at `f97c08e6` itself and one
with [[BPR-1032]]'s measurement change applied
(`build/bpr1032-memory/{baseline,candidate}-cache/history-1.log`).

- **Every refresh stalls, not one.** All 31 `epoch_refresh_N` phases have a
  largest GUI-thread gap of about 167 ms: the median is 166.5 ms in one run
  and 167.4 ms in the other, and the worst single gaps are 173.0 and
  206.8 ms. The budget is 100 ms.
- **Duration is not the problem.** Every refresh completes in 8.2–8.4 s,
  inside its 12 s limit.
- **Source selection is fine.** The `epoch_source_selection_N` phases stay
  at about 30 ms.
- **It regressed.** The budget was set in `75d1494e` (2026-09-05), when the
  same course measured a 67.903 ms largest refresh gap. The comment beside
  the limit records that figure.
- **Not caused by BPR-1032.** The row fails identically without BPR-1032's
  change. The refreshes do not run that change's measurement code.

No open card recorded the row. It is registered only on macOS, inside
`if(APPLE)` in `tests/end_to_end/CMakeLists.txt`.

## Scope

- Find what an epoch refresh does on the GUI thread for about 167 ms at
  every refresh. Likely candidates are a model reset or a whole-list rebuild
  of the epoch line or selector as the catalogue grows to 32 heads, and a
  synchronous catalogue read. Measure it; do not guess.
- Find when it regressed from 68 ms. The recorded runs of this row and the
  Qt 6.11.2 upgrade (`92c4e159`) are the first places to look.
- Fix it at its cause. If some of the work has to stay on the GUI thread,
  bound it and batch it the way the pane measurement is batched.
- Do not loosen the 100 ms budget to pass. A budget change needs its own
  measured justification, as [[BPR-0967]] AC2 says.

## Acceptance

- `application_large_review_history` passes on macOS, with every refresh's
  largest GUI gap inside the 100 ms budget.
- The card names the cause and the commit, or the window, where the gap
  grew from 68 ms.
- A focused check fails on the pre-fix behaviour.

## Cause, commit and fix (bpr-main-mac, 2026-09-23)

**Cause.** `/usr/bin/sample` on the benchmark process, 40 s through the
refreshes, puts about 108 ms of GUI-thread time per refresh in one timer
turn. The turn runs `workbench_window::apply_conversation_anchor`, then
`github_conversation_panel::set_anchor_file` and `refresh_projection`, then
`QTreeView::doItemsLayout`, then `updateScrollBars`, then
`github_conversation_delegate::sizeHint` for every row. A refresh moves the
anchored file, so the panel re-projects its whole list of 2,000 posts, and
does so twice (`refresh_projection` calls `doItemsLayout` twice). A tree
view that scrolls by pixel sums every row's height for its scroll range.
Each `sizeHint` shaped the row's heading, body and facts in fresh
`QTextLayout`s and asked the style for the row's geometry
(`QFusionStyle::subElementRect` and `sizeFromContents`). The epoch and
window-read completions (about 40 ms each) are separate turns and never were
the longest gap.

**The commit.** It is `cc038748`, "feat(rfc0037): integrate first-PR reading
surfaces", from 2026-09-19. Each side of it was built in a scratch worktree
and the course run alone at each. At `cc038748^` (`24c9e90c`) the course
passes. At `cc038748` the refreshes' median worst gap is 176.7 ms (max
180.0), and the course fails. Both builds use the Qt 6.10.3 that was pinned
at the time (`build/bpr1043-bisect-{24c9e90c,cc038748}-history.log`). That
commit gave the panel its explicit `View->doItemsLayout()` calls in the
projection paths. **The Qt 6.11.2 upgrade is not the cause:** today's tree
rebuilt against Qt 6.10.3 fails the same way, with a median of 164.0 ms
(`build/bpr1043-qt610-history.log`). The earlier record's window is
consistent with this: the last pass was on 2026-09-17 at 08:09 (dsp3-int12),
and the first recorded failure, on 2026-09-21 at 16:38, was also the first
recorded run after `cc038748`.

**The fix** (`src/adapters/qt/src/github_conversation_delegate.cpp`). The
delegate remembers the row sizes it has measured. The key is everything a
size depends on: the heading, body and facts; the display text the style
lays out; the font; the width; whether the row is a reply; the option's
features and decoration size; the style; and the device pixel ratio and
logical DPI. A change to any of them measures afresh. A relayout of rows
already seen now costs a lookup each, not three text layouts and two style
calls. The cache starts again at 16,384 entries, so resizing the drawer
through many widths cannot grow it without bound. Painting is unchanged and
still lays out what it draws. The panel's two `doItemsLayout` calls stay:
with the sizes remembered, the second pass costs lookups, and the calls keep
the reading position logic `cc038748` wrote.

**Verification.**

- **The course.** `application_large_review_history` now passes
  (`build/bpr1043-history-final.log`, run alone after a rebuild at a
  starting load of 17.9). All 31 refreshes' worst gaps have a median of
  68.8 ms and a maximum of 75.3 ms, back where the budget was set (67.9 ms).
  The course is the focused check that fails on the pre-fix behaviour. It
  was red at `f97c08e6` on Qt 6.11.2 (median 166.5–167.4 ms) and at
  `d856371b` rebuilt on Qt 6.10.3 (164.0 ms). The initial
  `paged_review_and_git_load` gap also falls, from 292–309 ms to
  231–243 ms.
- **What is remembered is right.** A new `qt_github_conversation_panel` case,
  `test_row_sizes_follow_text_width_and_font`, checks that new text under the
  same identity, a narrower width and a larger font each re-measure. It also
  checks that every remembered size equals a fresh delegate's
  (`build/bpr1043-negative-controls.log`). Taking the width out of the key,
  or the font, fails that case by name. So does taking out the text:
  removing the body alone does not fail it, because the display text carries
  the body too, but removing both does.
- **Integration** (`build/bpr1043-integration.log`, `-j4`): 22 of 25 rows
  pass. They include every conversation, thread-map, composed-GitHub,
  composed-Markdown, workbench-window and public-preview conversation row,
  the workbench and round-trip composition contracts, the anonymous
  public-preview public path, the ordinary large-review benchmark and
  `architecture_boundaries`. The three `qt_anonymous_public_preview_scale_*`
  rows fail at `GitHub.com's Connect, while it is not chosen, activates exactly
  its native HTTPS profile`. They fail the same way with this change removed,
  and [[BPR-1023]] (Backlog) already owns that course.
- `clang-format` reports the three touched files clean, and `clang-tidy`
  reports nothing on a changed line.
