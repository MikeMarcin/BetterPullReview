---
schema: "implement-rfc.card.v1"
id: "BPR-1176"
title: "Create and reconcile the resulting PR without duplication"
status: "In Progress"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1175"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-05", "RC59-07", "RC59-10"], "pack_alias": "PACK-CYCLE-34", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-34** (live `BPR-1176`, allocated at import).

# BPR-1176: Create and reconcile the resulting PR without duplication

## Outcome and bounded work

Create the PR only after the exact new head is verified. Reconcile matching head/base/repository/account after timeout; distinguish uniquely evidenced creation from coincidental/ambiguous matches. Re-read actual head/base for concurrent drift.

## Acceptance and direct evidence

Create success, lost response, existing foreign PR/branch, multiple matches, base/head movement, capability/permission refusal and restart. One separately approved disposable GitHub/GHES target course per claimed supported host.

Criteria: RC59-05, RC59-07, RC59-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
