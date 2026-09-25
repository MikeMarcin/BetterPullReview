---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-23"
title: "Reconcile source correspondence and explicit reviewer verification"
status: "Backlog"
rfcs: ["RFC-BPR-0057"]
depends_on: ["PACK-CYCLE-11", "PACK-CYCLE-21", "PACK-CYCLE-22"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "manual-iteration", "acceptance": ["RC57-03", "RC57-06", "RC57-07"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-23: Reconcile source correspondence and explicit reviewer verification

## Outcome and bounded work

Observe current anchors against result material with explicit uncertainty, preserving original code. Separate reported addressed from reviewer verified, material-bound verification history and recheck-needed. Create next drafts from explicit carry-forward selection, not automatic history dumps.

## Acceptance and direct evidence

Repeated/moved/deleted source, illustrative fixes, already-present exact replacement, relevant material change after verification, missing source and amended item revision. No code disappearance or test claim automatically verifies.

Criteria: RC57-03, RC57-06, RC57-07.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
