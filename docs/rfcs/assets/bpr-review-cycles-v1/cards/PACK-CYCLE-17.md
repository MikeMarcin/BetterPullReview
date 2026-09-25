---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-17"
title: "Implement complete Markdown and clipboard/file rendering"
status: "Backlog"
rfcs: ["RFC-BPR-0056"]
depends_on: ["PACK-CYCLE-16"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-03", "RC56-04", "RC56-08"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-17: Implement complete Markdown and clipboard/file rendering

## Outcome and bounded work

Render the same round into readable Markdown and plain clipboard/file bytes with stable IDs, useful source excerpts and dynamic fences. Preserve or explicitly encode non-text patch bytes. Enforce output budgets/completeness without silently dropping items.

## Acceptance and direct evidence

Eight-item/two-suggestion export, nested code fences, empty replacement, invalid text, selected context deduplication, sensitive absolute path omission, save conflict and over-limit round. Compare all selected item IDs and exact bytes.

Criteria: RC56-03, RC56-04, RC56-08.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
