---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-09"
title: "Feed full source and exact cycle comparisons into the workbench"
status: "Backlog"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0057"]
depends_on: ["PACK-CYCLE-07", "PACK-CYCLE-08"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC54-08", "RC54-09", "RC57-02"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-09: Feed full source and exact cycle comparisons into the workbench

## Outcome and bounded work

Adapt retained cycle manifests to existing file/material/diff services. Add true single-source observations and exact comparison pairs without fake PR epochs. Page tree/source requests, keep stable previous content and refuse missing/corrupt blobs rather than substituting live paths.

## Acceptance and direct evidence

Core/host tests for R0 full source, R0->R1, content-equivalent paths, missing pins, late material completion, expanded scope and cancellation. Verify real shared diff path, not a new toy comparator.

Criteria: RC54-08, RC54-09, RC57-02.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
