---
schema: "implement-rfc.card.v1"
id: "PACK-CYCLE-42"
title: "Measure large-project responsiveness and human steering benefit"
status: "Backlog"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0061"]
depends_on: ["PACK-CYCLE-24", "PACK-CYCLE-29"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC54-08", "RC61-10"], "alias_requires_live_allocation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# PACK-CYCLE-42: Measure large-project responsiveness and human steering benefit

## Outcome and bounded work

Measure named-corpus cold/warm browse/capture/freeze/export/compare latency and memory. Compare one eight-item/two-suggestion round against manual context assembly in a chosen harness, recording fixed task/source/settings and preparation/steering/rework outcomes.

## Acceptance and direct evidence

Hardware/build/data-size p50/p95 report, bounded input responsiveness, no silent omission at limits, human turn/reconstruction counts and honest mixed/negative findings. No blanket token savings or benchmark superiority claim.

Criteria: RC54-08, RC61-10.

## Ownership and constraints

Use the owning RFC and [integration map](../supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
