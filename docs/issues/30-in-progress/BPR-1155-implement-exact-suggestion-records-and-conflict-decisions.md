---
schema: "implement-rfc.card.v1"
id: "BPR-1155"
title: "Implement exact suggestion records and conflict decisions"
status: "In Progress"
rfcs: ["RFC-BPR-0055"]
depends_on: ["BPR-1153", "BPR-1150"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-04", "RC55-05", "RC55-06"], "pack_alias": "PACK-CYCLE-13", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-13** (live `BPR-1155`, allocated at import).

# BPR-1155: Implement exact suggestion records and conflict decisions

## Outcome and bounded work

Add byte-based proposal records and before/after material. Support insertion/deletion, exact versus illustrative intent and observed already-applied edits. Implement pure all-edit preflight with whole-blob preconditions and conservative overlap/same-boundary rejection.

## Acceptance and direct evidence

Original-coordinate multiple edits, overlapping ranges, two insertions, stale whole blob with locally matching text, CRLF/tab/Unicode preservation, illustrative non-apply and already-present no-double-apply cases.

Criteria: RC55-04, RC55-05, RC55-06.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
