---
schema: "implement-rfc.card.v1"
id: "BPR-1174"
title: "Implement finite publication authorization and recovery journal"
status: "In Progress"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1172", "BPR-1145"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-06", "RC59-07"], "pack_alias": "PACK-CYCLE-32", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-32** (live `BPR-1174`, allocated at import).

# BPR-1174: Implement finite publication authorization and recovery journal

## Outcome and bounded work

Persist exact candidate/destination/branch/body/identity plan, authorization generation and per-effect journal before sends. Model prepared/upload/ref/PR/linked/uncertain states. Reuse existing authority patterns without treating old comment grants as branch grants.

## Acceptance and direct evidence

Plan invalidation after title/body/source/account/base change, forged approval import, cancellation, operation replay, lost receipt and restart. Read-only recover does not silently resume remaining writes.

Criteria: RC59-06, RC59-07.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
