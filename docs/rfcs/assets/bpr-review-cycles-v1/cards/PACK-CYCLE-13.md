---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-13"
title: "Implement exact suggestion records and conflict decisions"
status: "Backlog"
rfcs: ["RFC-BPR-0055"]
depends_on: ["PACK-CYCLE-11", "PACK-CYCLE-08"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "authoring", "acceptance": ["RC55-04", "RC55-05", "RC55-06"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-13: Implement exact suggestion records and conflict decisions

## Outcome and bounded work

Add byte-based proposal records and before/after material. Support insertion/deletion, exact versus illustrative intent and observed already-applied edits. Implement pure all-edit preflight with whole-blob preconditions and conservative overlap/same-boundary rejection.

## Acceptance and direct evidence

Original-coordinate multiple edits, overlapping ranges, two insertions, stale whole blob with locally matching text, CRLF/tab/Unicode preservation, illustrative non-apply and already-present no-double-apply cases.

Criteria: RC55-04, RC55-05, RC55-06.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
