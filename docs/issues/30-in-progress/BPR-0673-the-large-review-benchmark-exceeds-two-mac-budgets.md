---
schema: "implement-rfc.card.v1"
id: "BPR-0673"
title: "The large-review benchmark exceeds two of its Mac budgets"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
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
[BPR-0668](../60-done/BPR-0668-a-failed-comparison-leaves-an-empty-workbench-that-says-nothing-is-open.md)'s
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
