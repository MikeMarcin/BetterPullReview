---
schema: "implement-rfc.card.v1"
id: "BPR-0870"
title: "Windows: the syntax pane course fails because the line that opened the block comment has scrolled off the top"
status: "Ready"
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
