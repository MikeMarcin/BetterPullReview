---
schema: "implement-rfc.card.v1"
id: "BPR-1153"
title: "Add source-attached private feedback and related locations"
status: "Ready"
rfcs: ["RFC-BPR-0055"]
depends_on: ["BPR-1145", "BPR-1151"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-01", "RC55-02", "RC55-10"], "pack_alias": "PACK-CYCLE-11", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-11** (live `BPR-1153`, allocated at import).

# BPR-1153: Add source-attached private feedback and related locations

## Outcome and bounded work

Implement versioned local feedback threads with immutable primary/related source anchors and range/file/module/cycle subjects. Reuse or extract the narrow private-thread seam from RFC0023 without activating its whole deferred program.

## Acceptance and direct evidence

One multi-file discussion, file/module notes without fake line zero, historical/unchanged anchors, repeated source text, private lineage and paged thread persistence; prove shared records rather than parallel schemas.

Criteria: RC55-01, RC55-02, RC55-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
