---
schema: "implement-rfc.card.v1"
id: "BPR-1972"
title: "Diagnose retained-workspace storage admission refusals precisely"
status: "Ready"
rfcs: []
depends_on: ["BPR-1969"]
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"mandate_revision":"retained-workspace-continuation-r1","reservation":"docs/issues/_reservations/BPR-1972--BPR-1972--c187b3b0f7ca.json","followup_of":"BPR-1969","origin":"Owner reports corrected build still refuses at 0:03 with retained storage budget or verification sentence; selected workspace earlier measured100800entries/8.4GiB.","milestone_plan":{"id":"bpr-1972-retained-budget","path":"docs/rfcs/plans/bpr-1972-retained-budget/milestone-plan.json"}}
acceptance: []
---

# Diagnose retained-workspace storage admission refusals precisely

The owner reports the BPR-1969 build reaches the next pre-dispatch check and says:
"Retained workspace storage reached its 16 GiB or 262144-entry bound, or could not
be verified. Existing work was kept; no question was sent."
Do not infer a stale build or assume the selected tree exceeds a limit. The adopted
RFC-0096 contract bounds the whole profile workspace neighbourhood, not one session.
Preserve its 16 GiB/262144 aggregate admission semantics and protected files.

## Acceptance

- AC1. Determine whether the reported refusal is aggregate bytes, aggregate entries,
  traversal/metadata failure, or a false accounting defect, using actual product
  evidence when available. Correct any reproduced false refusal; retain the exact
  observation and any inaccessible-root limitation rather than inventing a cause.
- AC2. Below-bound workspace inventories admit native continuation; the aggregate
  byte and entry limits still refuse before agent dispatch, with no deletion,
  rematerialization, secret/path logging or silent budget bypass. Tests cover the
  selected-tree/whole-neighbourhood distinction, ordinary files and no-follow links.
- AC3. A refusal separately names bytes, entries or unverifiable inventory in safe
  text and an exportable stable diagnostic. Counts identify the inventory observed;
  cancellation settles as cancelled. Any traversal failures remain explicit.
- AC4. Focused host/accounting and composed native/tour continuation checks pass;
  relevant documentation and evidence explain aggregate scope, exact candidate,
  failures, actual limits and any remaining owner-only observation.

## Out of scope

Increasing or bypassing the aggregate cap; automatic reclamation of protected
session trees; changing full-history materialization or the BPR-1969 pinned-object
probe; altering another agent's BPR-1971 replay diagnosis.
