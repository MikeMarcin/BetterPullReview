---
schema: "implement-rfc.card.v1"
id: "BPR-2167"
title: "What changed is grouped by area, and a merge of main is one line"
status: "Backlog"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2166"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"criteria":["AC07"],"origin":"imported 2026-11-13 from the owner's Ask design (docs/design/ask-v1); cards cut at import"}
---

# BPR-2167: What changed is grouped by area, and a merge of main is one line

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md); milestone `reports-in-a-file-tab` of the [plan](../../rfcs/plans/ask-v1/MILESTONES.md).

## Scope

Core: commit listing folds a merge of the base branch (and a rebase onto it, Q11) into one line with its count and whether it changed this pull request's files; What changed groups by area. Applied wherever commits are listed for an agent answer or report.

## Acceptance

- **AC07:** Wherever commits are listed, a merge of the base branch is one line with its commit count and whether it changed this pull request's files, and the other commits are grouped by area.
