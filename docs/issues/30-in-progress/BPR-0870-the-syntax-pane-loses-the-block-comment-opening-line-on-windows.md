---
schema: "implement-rfc.card.v1"
id: "BPR-0870"
title: "Windows: the syntax pane course fails because the line that opened the block comment has scrolled off the top"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-0653-implement","found_at":"20d99914","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0870: Windows: the syntax pane course fails because the line that opened the block comment has scrolled off the top

## Defect

`qt_syntax_pane_composition` fails on the Windows station at the assertion
"the line that opened the block comment has scrolled off the top". It passed on
that station in August.

It is not caused by [[BPR-0653]]: the station reports it at `20d99914` and the
course exercises the **diff workbench**, not the strip the title-row work
changed. It is Windows-only as far as anything shows — the macOS integrator
and the Linux full sweep at `a1864fa2` did not report it among their failures,
and the Linux sweep's eleven reds are all carded as BPR-0854 to BPR-0861.

The assertion is about a highlighter's continuation state across a scroll
boundary: a block comment opened above the viewport must still colour the
visible lines below it, and the course scrolls the opening line out of view to
prove the pane does not depend on seeing it. A Windows-only failure of that
points at how far the pane actually scrolled — a platform difference in row
height, viewport height or scrollbar metrics could leave the opening line
still visible, in which case the course never reaches the state it means to
test, and the assertion is right to complain.

## Scope

- On the Windows station, report what the course actually achieved: the first
  visible block number after the scroll, the opening line's block number, the
  viewport height, the row height and the scrollbar's range and value, at the
  scales the course uses.
- Separate the two possibilities and say which it is:
  - the scroll did not move as far as the course assumes on Windows, so the
    opening line is still visible and the precondition was never met (a course
    defect: scroll until the opening line is provably outside the viewport,
    rather than by a fixed amount); or
  - the pane really does lose the highlighter's continuation state on Windows
    (a visible product defect — a reviewer scrolling a long block comment
    would see the colouring break).
- Fix that side. If it is the course, make it assert the precondition it needs
  before asserting the consequence, so the next platform difference fails by
  name instead of at the conclusion.
- A `src/app` change reseals the round-trip pins with a recorded review.

## Acceptance

- `qt_syntax_pane_composition` passes on Windows, and still passes on macOS
  and Linux.
- The card names whether the product lost continuation state or the course's
  scroll precondition was platform-dependent.
- If it was the product, the colouring is shown correct in the rendered pane
  with the opening line scrolled away, not only in a model test.

## Windows run at the merged head, 2026-09-17 (bpr-win, task-20260917-win-merged-head-runs)

The head is `2b2e5549d81c4f9ba92cd93d542b5e6064b68c2e`. The full row-by-row sweep
(362 rows, 309 passed, 53 failed) is `docs/verification/dsp4-windows-merged-head-sweep.md`.

Still red at the merged head: `qt_syntax_pane_composition` fails at `FAILED: the
line that opened the block comment has scrolled off the top, so nothing on
screen says the marker line is a comment`. It uses the diff workbench, not the
strip, and passed on this station in August.

## Cause: the course scrolled before the pane had an axis to scroll by (bpr-main-win, 2026-09-23)

**The course's scroll precondition was platform-dependent. The product did
not lose continuation state.**

A local probe (not committed) printed the state at the scroll on Windows
(`build/dev`, MSVC 19.51 Debug, offscreen):

```text
marker=403 answer=406 top=0 shared[min=0 max=100000 value=99261 page=10]
pane-bar[min=0 max=365 value=0] viewport=638 row=14 blocks=409
before-settle geometry_pending=1 shared-enabled=0
```

The course set the shared scrollbar to 99,261 of 100,000, but the left pane's
own scrollbar stayed at 0, so the first visible block was 0. The shared
scrollbar moves the panes only once the publication's native measurement has
given the comparison scroll controller an axis. Until then its `valueChanged`
handler returns early (`comparison_scroll_controller.cpp:65`), and the bar is
disabled. The course waited four rounds of events after publishing. That was
enough on macOS and Linux. On Windows the measurement was still pending when
it scrolled, so the precondition was never reached.

**The fix is in the course** (`tests/adapters_qt/syntax_pane_composition_tests.cpp`).
Before it scrolls, it waits until `geometry_pending()` is false, with a
5-second deadline, and asserts that by name: "the publication's native
measurement completes before the course scrolls". This is the settle that
`diff_workbench_tests.cpp` and `workbench_window_tests.cpp` already use. The
existing "the line that opened the block comment has scrolled off the top"
check still guards the scroll itself, and the colouring checks are
unchanged.

**Rows (`build/dev`, offscreen):**

| Run | Measurement at the scroll | Result |
| --- | --- | --- |
| Probe with the settle disabled | `geometry_pending=1`, shared bar disabled | Fails at the scrolled-off check |
| Probe with the settle | `geometry_pending=0`, shared bar enabled | Passes |
| `qt_syntax_pane_composition` as registered (contained), with the fix | | Passes 5 of 5 (`--repeat until-fail:5`), 0.72–0.75 s |

The records are station logs under `build/logs/bpr-0870-main-win/` and are
not committed. On macOS and Linux, where the measurement was already done
after four rounds of events, the wait returns after one more round. No run
was made on those platforms from this station. `clang-format --dry-run
--Werror` is clean. No `src/app` file changed, so no pins move.
