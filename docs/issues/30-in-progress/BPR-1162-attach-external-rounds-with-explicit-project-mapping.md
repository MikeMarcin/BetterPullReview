---
schema: "implement-rfc.card.v1"
id: "BPR-1162"
title: "Attach external rounds with explicit project mapping"
status: "In Progress"
rfcs: ["RFC-BPR-0056", "RFC-BPR-0057"]
depends_on: ["BPR-1160", "BPR-1146", "BPR-1147"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "portable-rounds", "acceptance": ["RC56-05", "RC57-08"], "pack_alias": "PACK-CYCLE-20", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-20** (live `BPR-1162`, allocated at import).

# BPR-1162: Attach external rounds with explicit project mapping

## Outcome and bounded work

Allow opening a portable review without an account/agent. Validate immutable foreign IDs and explicit logical-project/source mapping before optional association; edits fork local authoring with provenance. Make readable prose usable without requiring structured return.

## Acceptance and direct evidence

Second local profile/import, same alias in two projects, invalid root hint, mapping to mismatched source, repeat import and read-only failure preservation. A bundle never applies code or overwrites sender identity.

Criteria: RC56-05, RC57-08.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
