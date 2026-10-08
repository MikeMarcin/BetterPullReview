---
schema: "implement-rfc.card.v1"
id: "BPR-1325"
title: "Windows: the initial paged review and Git load's input gap straddles its 100 ms budget"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1293's Windows station run, 2026-10-07","reservation":"docs/issues/_reservations/BPR-1324--BPR-1325--7765882b27a6.json","related_cards":["BPR-1293","BPR-1295","BPR-1297"],"kind":"investigation"}
---

# BPR-1325: Windows: the initial paged review and Git load's input gap straddles its 100 ms budget

## Observation

On the Windows station, in the optimized tests-on tree (`build/bpr0967-release`,
MSVC `/O2`, `ctest -j1`), two large-review rows fail some runs and pass others.
Both fail on one enforced budget:
`paged_review_and_git_load_input_gap`, the longest event-loop pulse interval
while the review and its Git history first load, against its 100 ms limit.

| Run (at `399aade8e`, 2026-10-07) | `application_first_tab_comparison` | `application_large_review_responsiveness` |
|---|---|---|
| 1 | fail, 110.7 ms | fail, 123.4 ms |
| 2 | fail, 104.1 ms | fail, 121.0 ms |
| 3 | fail, 119.1 ms | pass |
| 4 | pass | pass |

The benchmark reports "fixture input target missed outside the platform's
named phases" and "Mac fixture responsiveness budget exceeded". Every other
phase stays inside its budget, including [[BPR-1293]]'s Conversations phases.
The phase's 95th percentile is about 6 ms; one long pulse crosses the limit.
Windows' `KnownInputMisses` is empty.

The same rows passed in [[BPR-1295]]'s Windows run at `d8f4a67b7`. That run
kept no measurements, since ctest prints output only on failure, so the margin
there is unknown.

## Outcome

Find the work that runs on the decision lane during the initial load, long
enough to produce a 100–125 ms pulse on this station. Either move it off that
lane or bound it, so the phase stays inside 100 ms on Windows. Alternatively,
if the long pulse is the platform's own and not the application's, record that
with evidence and decide what the budget names on Windows. Neither the budget
nor the known-miss list is loosened without that evidence.

## Notes

- A per-pulse trace of the phase (which posted work ran in the longest pulse)
  is the first step. The benchmark's diagnostic log relays its last records on
  failure.
- Compare against `d8f4a67b7` with measurements kept, so it is clear whether
  this is new work since then or an old, thin margin.
- macOS and Linux show no miss; this card is Windows-only.
