---
schema: "implement-rfc.card.v1"
id: "BPR-1422"
title: "Native pane Find controls, decorations and exact reveal"
status: "In Progress"
rfcs: ["RFC-BPR-0077"]
depends_on: ["BPR-1420"]
metadata: {"milestone":"pane-find","source_id":"DRAFT-0018","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
---

# BPR-1422: Native pane Find controls, decorations and exact reveal

## Acceptance

- **AC1:** AC01 and AC02 pass through the shipped focused split/unified source reader, including offscreen unchanged source, selection/caret seeding, multiline options, fixed selection, Unicode/line-ending exact selection, invalid regex and wrap/focus.
- **AC2:** View > Find command availability, editor/modality ownership and native layouts use the required frontend-design skill; visible highlights and scroll-track markers do not alter source/diff geometry.
- **AC3:** Match traversal/reveal through the shared controller performs no rescan/recompile/read/diff; palette, accessible counts/status and narrow-width rendering are verified.

## Continuation

Import only under `find-and-find-in-files-v1-r1`; implementation and acceptance remain pending.
The [import checkpoint](../_checkpoints/find-and-find-in-files-v1/import-r1.md) records
scope, preserved work, verification and the next action.
