---
schema: "implement-rfc.card.v1"
id: "BPR-1164"
title: "Append manual and structured recipient responses safely"
status: "Ready"
rfcs: ["RFC-BPR-0057"]
depends_on: ["BPR-1158", "BPR-1162"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "manual-iteration", "acceptance": ["RC57-04", "RC57-05", "RC57-08", "RC57-10"], "pack_alias": "PACK-CYCLE-22", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-22** (live `BPR-1164`, allocated at import).

# BPR-1164: Append manual and structured recipient responses safely

## Outcome and bounded work

Implement optional bounded response schema/import and manual prose attachment. Bind exact round/digest/item membership and record claimed versus observed author/evidence. Idempotently append reports without editing instructions, source or reviewer state.

## Acceptance and direct evidence

Partial/multi-recipient/prose reports, self-reported model, wrong round digest, unknown item, same report replay, conflicting ID reuse, forged verification flag, executable evidence text and late adapter output.

Criteria: RC57-04, RC57-05, RC57-08, RC57-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
