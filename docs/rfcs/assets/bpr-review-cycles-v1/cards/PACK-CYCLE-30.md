---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-30"
title: "Plan selected outgoing content over an explicit Git base"
status: "Backlog"
rfcs: ["RFC-BPR-0059"]
depends_on: ["PACK-CYCLE-21", "PACK-CYCLE-23"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-01", "RC59-03"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-30: Plan selected outgoing content over an explicit Git base

## Outcome and bounded work

Implement R0/Rn/P/T selection, preexisting-work versus cycle-improvement inventory and candidate preview. Start from the complete P tree and overlay explicit selected changes; preserve uncaptured paths and identify dependency/conflict gaps.

## Acceptance and direct evidence

Dirty starting work, unrelated edits, module-only scope, scope extension, deletion/rename/mode change, empty candidate, moved target and iteration-only patch depending on omitted initial changes.

Criteria: RC59-01, RC59-03.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
