---
schema: "implement-rfc.card.v1"
id: "BPR-1349"
title: "A feedback count refresh rebuilds the whole snapshot on the window thread"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"performance","found_by":"bpr-main-linux, BPR-1094's measurements, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1349--BPR-1351--2371e399c373.json","related_rfcs":["RFC-BPR-0049"],"related_cards":["BPR-1094","BPR-1087","BPR-1089"]}
---

# BPR-1349: A feedback count refresh rebuilds the whole snapshot on the window thread

## Finding

[[BPR-1094]] measured the label counters, at `269f7ccff`. The setup:
- i9-10900KF, WSL2, GCC 15 `-O3`.
- Row `application_feedback_counter_costs`.

An unchanged refresh, a root label edit and a resolve each cost about as much
as the first observation, even though they parse 0 or 1 body and re-tally at
most 5 scopes:

| Work | 1k conversations | 10k conversations |
|---|---|---|
| First observation | 8.5 ms | 90.9 ms, 415k allocations / 59 MB |
| Unchanged snapshot | 8.2 ms | 91.1 ms, 52.5 MB |
| Root edit | 9.4 ms | 92.6 ms, 56 MB |

The per-snapshot pass in `publish_overview` runs on the window's thread. It
copies every body into the adapter's posts, rebuilds the annotation set post
by post, rebuilds the population, compares every unit and re-tallies the pull
request scope over every unit. 10k is the read model's own limit.

## Outcome

A refresh whose snapshot changed little costs work in proportion to what
changed, or the remaining linear pass moves off the window's thread, so that
a 10k-conversation pull request refreshes within the input budget.

## Acceptance

- Re-measure with `application_feedback_counter_costs` on the same workloads.
  The unchanged-snapshot and root-edit rows drop clearly below the first
  observation at 10k, with the hardware and build recorded.
- Counts, drill-down and annotations stay identical; BPR-1094's structural
  assertions and controls still hold.
- The 100 ms rows (`application_large_review_responsiveness`,
  `application_first_tab_comparison`) pass.

## Boundaries

No live GitHub. Only worth doing if pull requests near 10k conversations
matter to the owner; at 1k the refresh takes 8 to 9 ms.
