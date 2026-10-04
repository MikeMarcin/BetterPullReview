---
schema: "implement-rfc.card.v1"
id: "BPR-1166"
title: "Prove the complete manual two-round workflow"
status: "Ready"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0061"]
depends_on: ["BPR-1161", "BPR-1163", "BPR-1165"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "manual-iteration", "acceptance": ["RC56-03", "RC57-01", "RC61-07"], "pack_alias": "PACK-CYCLE-24", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-24** (live `BPR-1166`, allocated at import).

# BPR-1166: Prove the complete manual two-round workflow

## Outcome and bounded work

Compose the real desktop path from source selection through eight notes/two suggestions, Copy, external edits, restart, Capture, diff, verify, Save next round and another capture. Keep optional agent components unavailable throughout this course.

## Acceptance and direct evidence

Record native interactions, producing build, frozen payloads and source hashes. Show no callback/report dependency, no lost notes, no hidden AI activity and correct R0/R1/R2 comparisons; this is the first end-to-end value milestone.

Criteria: RC56-03, RC57-01, RC61-07.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
