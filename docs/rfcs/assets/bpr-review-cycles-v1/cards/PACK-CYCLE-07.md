---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-07"
title: "Capture scoped quiescent revisions without source writes"
status: "Backlog"
rfcs: ["RFC-BPR-0054"]
depends_on: ["PACK-CYCLE-03", "PACK-CYCLE-06"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC54-02", "RC54-04", "RC54-05"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-07: Capture scoped quiescent revisions without source writes

## Outcome and bounded work

Implement bounded enumeration, raw-byte capture, before/after checks, finite instability retry and staged immutable manifests. Preserve exclusions, explicit deletions, index/working-tree distinction and observed_stable/immutable_source quality. Never infer global atomicity from two scans.

## Acceptance and direct evidence

Hash source/index/refs before and after capture; hostile hook/filter/lazy-fetch canaries; concurrent writer and enumeration changes; scope extension and partial deletion cases; cancellation with no visible partial revision.

Criteria: RC54-02, RC54-04, RC54-05.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
