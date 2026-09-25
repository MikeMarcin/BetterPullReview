---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-41"
title: "Run hostile input, data-loss and mixed-cycle recovery corpus"
status: "Backlog"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0056", "RFC-BPR-0060"]
depends_on: ["PACK-CYCLE-10", "PACK-CYCLE-18", "PACK-CYCLE-23", "PACK-CYCLE-36", "PACK-CYCLE-39"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC54-03", "RC56-05", "RC60-05", "RC60-06"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-41: Run hostile input, data-loss and mixed-cycle recovery corpus

## Outcome and bounded work

Combine filesystem, archive, canonical payload, source correspondence, response and publication failure cases at production integration seams. Verify containment, retained evidence, truthful unknowns and unrelated PR/cycle isolation.

## Acceptance and direct evidence

Every failure scenario in verification/SCENARIOS.json has a concrete test/check/result. Include disk full, corrupt retained bytes, stale drafts, rebind, duplicate JSON, case collisions, missing public/private history and uncertain delivery.

Criteria: RC54-03, RC56-05, RC60-05, RC60-06.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
