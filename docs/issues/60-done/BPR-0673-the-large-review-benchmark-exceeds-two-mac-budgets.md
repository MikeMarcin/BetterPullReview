---
schema: "implement-rfc.card.v1"
id: "BPR-0673"
title: "The large-review benchmark exceeds two of its Mac budgets"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
acceptance: [{"criterion":"AC1 application_large_review_history passes on macOS, alone and under a parallel matrix","outcome":"pass","revision":"8b002ccde89557a47dbc88ba4b3fb5ed53048475 plus the src/adapters/qt change committed by this finish","evidence":{"kind":"command","detail":"Alone: 4 slice runs (worst 64.2-67.2 ms) and the integrated head run (worst 68.6 ms, refreshes 2-31 at 54-61 ms) against the 100 ms limit (build/r2-logs/dsp3-0673-after-history-alone-*.log, dsp3-0673-final-history-alone.log, dsp3-int12-history.log). Under a concurrent ctest -j 4 matrix of 12 Qt rows, 17 rounds: pass, worst 67.2 ms (dsp3-0673-after-history-parallel.log). Before: 93.5 ms."}},{"criterion":"AC2 the card names what was holding the loop and what changed","outcome":"pass","revision":"same","evidence":{"kind":"artifact","detail":"Recorded on this card: cards view re-measuring 1000 cards on alternating widths, files_tree path decoding, per-relayout file identity hashing; removed redundant work on the loop (two-width measurement cache, per-forest identities, hash lookups, path-to-row hash); independent rfc-reviewer no blocker; 15/15 Qt and composition rows pass (dsp3-int12-qt.log)."}}]
---

# The large-review benchmark exceeds two of its Mac budgets

`application_large_review_history` fails on macOS at 42cec0f6, alone and
under load, on its responsiveness bound rather than its duration. Each
epoch refresh finishes inside its 12 s budget, around 9 s, but the longest
gap between event-loop pulses during the refresh is about 270 ms against a
100 ms limit:

| refresh | longest pulse gap | elapsed |
|---|---|---|
| 7 | 278 ms | 9.3 s |
| 15 | 277 ms | 9.1 s |
| 12 | 272 ms | 9.5 s |

Thirty-one refreshes were measured and the worst five are all near 270 ms,
so this is the shape of the work, not a stray sample. For a quarter of a
second at a time, during a refresh of a large review, the window does not
answer the reviewer.

Not caused by the RFC-BPR-0028 pack's presentation work: removing
[BPR-0668](BPR-0668-a-failed-comparison-leaves-an-empty-workbench-that-says-nothing-is-open.md)'s
status refresh and rebuilding leaves the failure exactly where it was. It
was observed failing at three heads in this session, including before that
change landed. Whether it predates the pack is not established; that is the
first thing to find out.

## Scope

- Establish when it started: run the course at the pack's base and bisect
  if it was green there. A build is about four minutes and the course is
  about six.
- Find what holds the loop for a quarter of a second inside a refresh. The
  course reports pulses with diff and workbench work per phase, which
  narrows it before any profiler is needed.
- Either break that work up or move it off the loop, and say which in the
  card.
- Owned paths: whatever the measurement names.

## Acceptance

- `application_large_review_history` passes on macOS, alone and under a
  parallel matrix.
- The card names what was holding the loop and what changed.

## Note, 2026-09-17 (dsp-002): the margin after BPR-0818

[[BPR-0818]] ended [[BPR-0751]]'s refresh hold, which had hidden review-wide
conversation roots, and moved the drawer's widening onto its own
event-loop turn. `application_large_review_history` passes on macOS at
`9eed16c1` and later, but only just. On a quiet run (load 1.07), refresh 32's
longest pulse was 93.3 ms against the 100 ms limit, and refreshes 20–31 reached
88–89 ms (`build/r2-logs/dsp2-int2-history.log`). Before BPR-0818 they held
at 65–71 ms. Re-projecting the 1000-thread drawer costs about 51 ms: card
adoption about 30 ms, thread-map adoption about 13.5 ms, model apply about
7 ms. If this card is picked up, that re-projection is the first measured
candidate to break up.

## What held the loop, and what changed (dsp-003)

These gaps were measured at `e7389518` with temporary scopes and a notify
hook, which have since been removed (`build/r2-logs/dsp3-0673-trace-before.log`,
`dsp3-0673-trace2-before.log`). The worst gaps had two separate causes.

1. **Refreshes 14 to 31 (about 84 ms): the deferred drawer widening, 52 to
   54 ms**, followed by the queued epoch and overview posts with no pulse in
   between.
   - Card adoption took 31 ms. Of that, 23 ms re-measured all 1000 cards:
     `conversation_cards_view.cpp` cleared its whole measurement cache on
     any width change, and the scroll bar alternates the content width
     between 284 and 266.
   - Thread-map adoption took about 13.5 ms, and the model apply about 7 ms.
2. **Refreshes 2 to 13 (67 to 72 ms) and refresh 32 (90 to 94 ms): the
   observation's terminal task.**
   - The overview publication took 34 ms, 24.7 ms of it in Qt, because
     `files_tree.cpp` decoded one path per entry and row pair, about 265,000
     times for 515 files.
   - Three read-state observations took about 9 ms each.
   - `clear_diff_selection` took 14.2 ms. Most of that was hashing every
     node's path with `file_identity_of` on each drawer re-projection and
     card relayout.

**What changed: the redundant work is gone, and nothing moved off the
loop.** All changes are in `src/adapters/qt`.

- The cards view keeps one set of measurements for each of the last two
  widths. Font, style, feed and resolution changes still clear both, and
  every hit still checks the card's content.
- The cards view and the panel compute each node's file identity once per
  adopted forest.
- Card relayout finds nodes and read state through hash lookups.
- `files_tree` builds one path-to-row hash, keeping the first row for a path
  as the old linear search did.

Rows, order, selection, focus and accessibility text are unchanged. A new
test, `test_alternating_widths_keep_both_widths_measured`, fails with the
two-width retention disabled (`dsp3-0673-negative-control.log`).

**Measured** on one arm64 Mac, Debug build, offscreen, Qt 6.10.3:

| Run | Worst refresh pulse | Load | Log |
|---|---|---|---|
| Course alone, `e7389518` (before) | 93.5 ms, refresh 32 | 3.8 to 3.0 | `dsp3-0673-baseline-history-1.log` |
| Alone, after, 3 runs | 66.0, 64.2, 67.2 ms | 2.1 to 1.6 | `dsp3-0673-after-history-alone-{1,2,3}.log` |
| Beside a concurrent `ctest -j 4` looping 12 Qt rows (17 rounds, all passing) | 67.2 ms | 1.6 to 3.7 | `dsp3-0673-after-history-parallel.log`, `dsp3-0673-after-parallel-matrix.log` |
| Alone, final source | 65.8 ms | 4.1 to 1.9 | `dsp3-0673-final-history-alone.log` |
| **Integrated, main `8b002ccd` with the change** | **68.6 ms (refresh 32); refreshes 2 to 31 at 54 to 61 ms** | 3.6 to 1.9 | `dsp3-int12-history.log` |

Source-selection worst pulses also fell, from 48.8 ms to 36 to 40 ms. At the
integrated head, 15 of 15 pass (`dsp3-int12-qt.log`):
`architecture_boundaries`, the workbench window, composed GitHub and thread
map scale rows, the cards view, conversation panel and files tree Qt tests,
`application_diff_workbench_public_path` and
`application_review_behaviour_contract`.

An independent `rfc-reviewer` found **no blocker**. It traced every input to
a measurement against a clearing or validating check, and confirmed the
pruning of both generations on adopt, the identity cache rewrites, that hash
maps are only looked up and never iterated, the first-row semantics, and the
two-width bound. Its non-blocking note is a test gap: nothing checks that a
content change at one width re-measures when the other width returns.

**Left for later, not criteria here.** The worst gap is now composition
work: three read-state observations and the overview projection, in
`src/app/src/github_application.cpp` (pinned). Thread-map adoption (13 ms)
and model apply (7 ms) remain in the widening. The course's first scope item,
finding when the slowdown started, was not done; the dsp-002 note narrowed
the question to the margin.
