---
schema: "implement-rfc.card.v1"
id: "BPR-1421"
title: "Exact retained-comparison corpus and streamed PR search"
status: "Ready"
rfcs: ["RFC-BPR-0077"]
depends_on: ["BPR-1420"]
metadata: {"milestone":"exact-pr-corpus","source_id":"DRAFT-0017","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
---

# BPR-1421: Exact retained-comparison corpus and streamed PR search

## Acceptance

- **AC1:** Historical membership derives from exact retained trees when no accepted catalogue proves the pair; missing trees/catalogue are unavailable rather than substituted.
- **AC2:** All changed entries, both exact paths and full documents participate independently of Files filters, receipts or opened state; added/removed/rename/copy and unsupported sides stay honest.
- **AC3:** Exact blob acquisition remains on the epoch actor, coalesces immutable reads across generations, yields behind selected navigation and pane Find, and keeps search inside existing cache limits.
- **AC4:** Results publish in defined path/side/offset order in bounded batches with precise partial/limit/retry coverage; core and host need no QApplication.

## Continuation

Import only under `find-and-find-in-files-v1-r1`; implementation and acceptance remain pending.
The [import checkpoint](../_checkpoints/find-and-find-in-files-v1/import-r1.md) records
scope, preserved work, verification and the next action.
