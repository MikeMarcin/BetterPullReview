---
schema: "implement-rfc.card.v1"
id: "BPR-1159"
title: "Implement complete Markdown and clipboard/file rendering"
status: "In Progress"
rfcs: ["RFC-BPR-0056"]
depends_on: ["BPR-1158"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-03", "RC56-04", "RC56-08"], "pack_alias": "PACK-CYCLE-17", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-17** (live `BPR-1159`, allocated at import).

# BPR-1159: Implement complete Markdown and clipboard/file rendering

## Outcome and bounded work

Render the same round into readable Markdown and plain clipboard/file bytes with stable IDs, useful source excerpts and dynamic fences. Preserve or explicitly encode non-text patch bytes. Enforce output budgets/completeness without silently dropping items.

## Acceptance and direct evidence

Eight-item/two-suggestion export, nested code fences, empty replacement, invalid text, selected context deduplication, sensitive absolute path omission, save conflict and over-limit round. Compare all selected item IDs and exact bytes.

Criteria: RC56-03, RC56-04, RC56-08.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
