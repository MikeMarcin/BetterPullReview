---
schema: "implement-rfc.card.v1"
id: "BPR-1247"
title: "Large review: accepted_metadata_render takes 382 ms against its 250 ms Mac fixture budget"
status: "Backlog"
rfcs: []
depends_on: []
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json","related_cards":["BPR-0967","BPR-1211"]}
---

# BPR-1247: Large review: accepted_metadata_render takes 382 ms against its 250 ms Mac fixture budget

## Observation

At `main` 83edd361b on macOS arm64 (dev preset, Debug), `application_large_review_responsiveness`
and `application_large_review_launch` fail with
`large-review-benchmark-failed: Mac fixture responsiveness budget exceeded`
(`tests/end_to_end/large_review_benchmark.cpp:1127`). The one failing budget
in the report is `CompletionBudgets` `accepted_metadata_render`: measured
382.2 ms, limit 250 ms. No input budget failed. BPR-1211's implementer saw
this row red at its baseline d25f17b69.

Not measured: when it started, whether a quiet machine passes (this run shared
the machine with a full ctest), and whether the Release build passes.
[[BPR-0967]] owns the optimized hardware budgets; this is the Debug Mac
fixture budget the row enforces.

## Outcome

Either the render is brought back under its budget, with the regressing change
named, or a measured, owner-accepted budget change is recorded. The budget is
not raised to make the row pass.

## Direct acceptance

- Both rows pass on macOS, or the card records the measured cause and the
  owner's budget decision.
