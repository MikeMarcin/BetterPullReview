---
schema: "implement-rfc.card.v1"
id: "BPR-1419"
title: "Search session identity and bounded host lifecycle"
status: "In Progress"
rfcs: ["RFC-BPR-0077"]
depends_on: []
metadata: {"milestone":"search-session-policy","source_id":"DRAFT-0015","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
---

# BPR-1419: Search session identity and bounded host lifecycle

## Acceptance

- **AC1:** The core stores only exact scope, query/request generations, compact result counts and cursor facts; no query text, source documents or result collections enter interaction state.
- **AC2:** The public Qt-free search host validates the 4 KiB query boundary, submits no work for empty input, separates independent pane/PR sessions and reports invalid input without retaining apparently current results.
- **AC3:** Reversed query/scope/authority completions and duplicates cannot publish; supersession, clear, close and shutdown cancel old work, preserve exactly-one terminals for admitted effects, and reach observable quiescence.
- **AC4:** Next/Previous traverses accepted counts in constant work, wraps explicitly and never calls the worker; incomplete coverage cannot become unqualified No results.

## Continuation

Import only under `find-and-find-in-files-v1-r1`; implementation and acceptance remain pending.
The [import checkpoint](../_checkpoints/find-and-find-in-files-v1/import-r1.md) records
scope, preserved work, verification and the next action.
