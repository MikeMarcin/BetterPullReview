---
schema: "implement-rfc.card.v1"
id: "BPR-1423"
title: "Composed PR Search drawer, result activation and invalidation"
status: "In Progress"
rfcs: ["RFC-BPR-0077"]
depends_on: ["BPR-1421","BPR-1422"]
metadata: {"milestone":"pr-search","source_id":"DRAFT-0019","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
---

# BPR-1423: Composed PR Search drawer, result activation and invalidation

## Acceptance

- **AC1:** AC03–AC07 pass through real Search-tab commands and key paths: historical membership, exact result activation/F4, sibling preservation, filters/deduplication, stale completion/reveal, qualified coverage and accessible native layout.
- **AC2:** Selection/reveal binds PR authority, catalogue, tab, comparison, exact path/side/blob/span and query generation; later reading gestures cancel it.
- **AC3:** Pane and PR query state remain independent and runtime-only; authority changes clear sensitive text/results and all newly admitted effects reach their terminal.

## Continuation

Import only under `find-and-find-in-files-v1-r1`; implementation and acceptance remain pending.
The [import checkpoint](../_checkpoints/find-and-find-in-files-v1/import-r1.md) records
scope, preserved work, verification and the next action.
