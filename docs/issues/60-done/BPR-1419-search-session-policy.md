---
schema: "implement-rfc.card.v1"
id: "BPR-1419"
title: "Search session identity and bounded host lifecycle"
status: "Done"
rfcs: ["RFC-BPR-0077"]
depends_on: []
metadata: {"milestone":"search-session-policy","source_id":"DRAFT-0015","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7efc8c2d547bc74208069f9e7331e0f0253bc107","evidence":{"kind":"observation","detail":"Core state is trivially copyable,1528bytes; exactscope/generation/options/count/cursor only, no query/material/results collections.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m1-scoped-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m1-self-review.json","docs/verification/RFC-BPR-0077.md"]}},{"criterion":"AC2","outcome":"pass","revision":"7efc8c2d547bc74208069f9e7331e0f0253bc107","evidence":{"kind":"observation","detail":"Public Qt-free host admits4096bytes, refuses4097/invalidUTF8 without copying excess or retaining current hits; empty performs no work; separate pane/PR host query state.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m1-scoped-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m1-self-review.json","docs/verification/RFC-BPR-0077.md"]}},{"criterion":"AC3","outcome":"pass","revision":"7efc8c2d547bc74208069f9e7331e0f0253bc107","evidence":{"kind":"observation","detail":"Core reverse/foreign/duplicate completions refused; public host101admitted edits101terminals with2worker calls, bounded reentrant ingress, cancelled coalescing, projection/worker/executor failures and clear/close/reset/shutdown reach quiescence.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m1-scoped-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m1-self-review.json","docs/verification/RFC-BPR-0077.md"]}},{"criterion":"AC4","outcome":"pass","revision":"7efc8c2d547bc74208069f9e7331e0f0253bc107","evidence":{"kind":"observation","detail":"1000public match traversals perform zero worker/effect calls; next/previous wraps accepted prefix; complete-empty alone projects NoResults, skipped/unsearched/error/limit stays qualified.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m1-scoped-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m1-self-review.json","docs/verification/RFC-BPR-0077.md"]}}]
---

# BPR-1419: Search session identity and bounded host lifecycle

## Acceptance

- **AC1:** The core stores only exact scope, query/request generations, compact result counts and cursor facts; no query text, source documents or result collections enter interaction state.
- **AC2:** The public Qt-free search host validates the 4 KiB query boundary, submits no work for empty input, separates independent pane/PR sessions and reports invalid input without retaining apparently current results.
- **AC3:** Reversed query/scope/authority completions and duplicates cannot publish; supersession, clear, close and shutdown cancel old work, preserve exactly-one terminals for admitted effects, and reach observable quiescence.
- **AC4:** Next/Previous traverses accepted counts in constant work, wraps explicitly and never calls the worker; incomplete coverage cannot become unqualified No results.

## Continuation

Implemented under `find-and-find-in-files-v1-r1` at product commit `34256fb55`,
integrated candidate `7efc8c2d547bc74208069f9e7331e0f0253bc107`. The
[verification ledger](../../verification/RFC-BPR-0077.md) and
[scoped integration](../_checkpoints/find-and-find-in-files-v1/m1-scoped-integration.json)
bind all four card criteria to the public Qt-free core/host composition.
Core state measures 1,528 bytes; 101 admitted rapid edits retire exactly once,
and 1,000 match traversals execute no worker. The exact-request negative
control fails as expected. Touched format/tidy22, documentation and both
architecture checks pass. The full core-only run records three unrelated
baseline/environment failures, with reproduced
[dispositions](../_checkpoints/find-and-find-in-files-v1/m1-integration-failures.json).

M1 adds no matcher, exact corpus, source span, native UI or performance claim.
All full-RFC AC01–AC08 remain pending. Next: [[BPR-1420]], shared matching.
No human decision or review server is held; all task-owned commands have ended.
