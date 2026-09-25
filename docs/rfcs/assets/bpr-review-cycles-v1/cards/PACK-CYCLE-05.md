---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-05"
title: "Implement local scope and disclosure policy primitives"
status: "Backlog"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0060"]
depends_on: ["PACK-CYCLE-02"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "domain", "acceptance": ["RC53-10", "RC60-04", "RC60-05"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-05: Implement local scope and disclosure policy primitives

## Outcome and bounded work

Add local workspace/round capability types, explicit origin/classification and channel policy decisions. Separate capture, freeze, export, implement instruction, response append, reviewer verification and publication authorization. Define bounded errors rather than implicit global access.

## Acceptance and direct evidence

Core policy table covering unknown folder origin, known enterprise data, denied channel, response-only grants, imported claimed approval and source snippets containing commands.

Criteria: RC53-10, RC60-04, RC60-05.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
