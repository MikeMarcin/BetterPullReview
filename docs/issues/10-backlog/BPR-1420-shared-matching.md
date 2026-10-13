---
schema: "implement-rfc.card.v1"
id: "BPR-1420"
title: "Shared Unicode source matcher and repository path filters"
status: "Backlog"
rfcs: ["RFC-BPR-0077"]
depends_on: ["BPR-1419"]
metadata: {"milestone":"shared-matching","source_id":"DRAFT-0016","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
---

# BPR-1420: Shared Unicode source matcher and repository path filters

## Acceptance

- **AC1:** Both commands share deterministic literal and bounded RE2-subset semantics: Unicode simple fold, Unicode word characters, nonoverlap, zero-width progress, multiline LF view and exact original UTF-8 spans.
- **AC2:** Include/exclude glob parsing follows comma/braces/classes/escaping and either-side include/exclude-wins rules; invalid patterns are refused before reads.
- **AC3:** Unified scanning follows actual version-contiguous source ranges, removes duplicate unchanged hits and cannot fabricate cross-version multiline matches; identical-blob same-entry deduplication is exact.
- **AC4:** Compile, scan, preview and result memory have explicit ceilings and cancellable bounded work; source-span tests need no QApplication.

## Continuation

Import only under `find-and-find-in-files-v1-r1`; implementation and acceptance remain pending.
The [import checkpoint](../_checkpoints/find-and-find-in-files-v1/import-r1.md) records
scope, preserved work, verification and the next action.
