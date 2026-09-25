---
schema: "implement-rfc.card.v1"
id: "BPR-1198"
title: "Reconcile epoch refresh with the active tab selected-file identity"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_at":"f6de40b633ea8b879d3643ad5a259169fd976975","plan":"docs/rfcs/plans/bpr1198-refresh-identity/milestone-plan.json","mandate":"user-unattended-r1"}
---

# BPR-1198: Reconcile epoch refresh with the active tab selected-file identity

## Defect and scope

The full Mac suite at `f6de40b6` expired in `application_review_behaviour_contract`, GHES seed, at stage 4 after 120076/120000 ms. Four file roots were present; the six review-wide roots were expected. This resembles terminal [[BPR-0818]], but [[BPR-1132]] now restores the active tab's exact file and comparison. Distinguish a legitimate selected view from a stale conversation anchor before correcting the product or course. Preserve 120000 ms.

## Acceptance

- Exact terminal observations distinguish host selection, active tab file/pair, displayed comparison and conversation anchor; name the cause.
- The public review-behaviour contract passes on macOS with fail-before/pass-after evidence; a focused view requires exact reconciled identities, and a genuinely unselected view still requires all six roots. Preserve BPR-0818 regression coverage and the 120000 ms budget.

## Authority

Standalone follow-up assigned by the unattended coordinator. Owner mandate `user-unattended-r1`; source baseline `f6de40b6`. New Windows/Linux runs are deferred by the owner. No native acceptance or RFC closeout is claimed.
