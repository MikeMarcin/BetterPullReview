---
schema: "implement-rfc.card.v1"
id: "BPR-0673"
title: "Epoch refresh stalls the event loop past its responsiveness bound"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
---

# Epoch refresh stalls the event loop past its responsiveness bound

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
