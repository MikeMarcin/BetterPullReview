---
schema: "implement-rfc.card.v1"
id: "BPR-1178"
title: "Exercise publication crash, drift and recovery matrix"
status: "Ready"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1177"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-07", "RC59-10"], "pack_alias": "PACK-CYCLE-36", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-36** (live `BPR-1178`, allocated at import).

# BPR-1178: Exercise publication crash, drift and recovery matrix

## Outcome and bounded work

Inject failure at every object/ref/PR boundary, including after a remote effect but before local receipt. Verify read-first recovery, idempotent association, no force/cleanup fiction and current authorization for remaining operations.

## Acceptance and direct evidence

Fixture matrix plus separately authorized live timeout/cancellation cases where feasible. Record confirmed remote effects and residual races. External fallback success does not pass native publication rows.

Criteria: RC59-07, RC59-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
