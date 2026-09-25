---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-10"
title: "Qualify capture recovery and retained-material budgets"
status: "Backlog"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0054"]
depends_on: ["PACK-CYCLE-07", "PACK-CYCLE-08"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC53-04", "RC54-04", "RC54-09"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-10: Qualify capture recovery and retained-material budgets

## Outcome and bounded work

Complete source-store install/recovery, pin accounting and bounded retention/capture failure paths. Add stable-capture diagnostics which explain observed quality without an every-round warning wizard.

## Acceptance and direct evidence

Kill/reopen at every candidate/install/catalog boundary, disk full, corrupted blob, protected quota exhaustion, retry exhaustion and missing-scope material. Confirm no lost existing cycle or invented atomicity.

Criteria: RC53-04, RC54-04, RC54-09.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
