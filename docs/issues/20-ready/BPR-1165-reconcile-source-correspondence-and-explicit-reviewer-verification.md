---
schema: "implement-rfc.card.v1"
id: "BPR-1165"
title: "Reconcile source correspondence and explicit reviewer verification"
status: "Ready"
rfcs: ["RFC-BPR-0057"]
depends_on: ["BPR-1153", "BPR-1163", "BPR-1164"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "manual-iteration", "acceptance": ["RC57-03", "RC57-06", "RC57-07"], "pack_alias": "PACK-CYCLE-23", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-23** (live `BPR-1165`, allocated at import).

# BPR-1165: Reconcile source correspondence and explicit reviewer verification

## Outcome and bounded work

Observe current anchors against result material with explicit uncertainty, preserving original code. Separate reported addressed from reviewer verified, material-bound verification history and recheck-needed. Create next drafts from explicit carry-forward selection, not automatic history dumps.

## Acceptance and direct evidence

Repeated/moved/deleted source, illustrative fixes, already-present exact replacement, relevant material change after verification, missing source and amended item revision. No code disappearance or test claim automatically verifies.

Criteria: RC57-03, RC57-06, RC57-07.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
