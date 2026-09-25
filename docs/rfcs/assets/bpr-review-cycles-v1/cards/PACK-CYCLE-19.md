---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-19"
title: "Expose share routes and exact delivery observations in the GUI"
status: "Backlog"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0061"]
depends_on: ["PACK-CYCLE-14", "PACK-CYCLE-15", "PACK-CYCLE-17"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-07", "RC56-10", "RC61-05", "RC61-06"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-19: Expose share routes and exact delivery observations in the GUI

## Outcome and bounded work

Compose feedback and Share feedback in the accepted native layout. Copy and Save are first-class, with selection/order/context preview; optional routes are capability-driven. Freeze+delivery may be one action without confusing copied/saved/read/acknowledged states.

## Acceptance and direct evidence

No-agent native share course, identical round Copy then Save, amendment after share, explicit destination/bytes, denied channel, oversized output and no per-item confirmation/second prompt. No automatic executable discovery or network on preview.

Criteria: RC56-07, RC56-10, RC61-05, RC61-06.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
