---
schema: "implement-rfc.card.v1"
id: "BPR-1146"
title: "Separate local runtime and adapt shared material interfaces"
status: "In Progress"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0060"]
depends_on: ["BPR-1144"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "domain", "acceptance": ["RC53-03", "RC53-06", "RC53-07"], "pack_alias": "PACK-CYCLE-04", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-04** (live `BPR-1146`, allocated at import).

# BPR-1146: Separate local runtime and adapt shared material interfaces

## Outcome and bounded work

Compose a local profile/runtime path independent of account enrollment and agent discovery. Add the tagged material bridge to the existing workbench/diff APIs. Isolate optional agent dependencies at the application seam early; record remaining packaging couplings for card 37.

## Acceptance and direct evidence

No-agent/no-account initialization, old PR migration and subject-switch tests, cross-authority cache refusal and link/dependency inspection of core cycle services.

Criteria: RC53-03, RC53-06, RC53-07.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
