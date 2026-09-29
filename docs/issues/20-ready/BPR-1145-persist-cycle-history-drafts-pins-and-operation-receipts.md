---
schema: "implement-rfc.card.v1"
id: "BPR-1145"
title: "Persist cycle history, drafts, pins and operation receipts"
status: "Ready"
rfcs: ["RFC-BPR-0053"]
depends_on: ["BPR-1144"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "domain", "acceptance": ["RC53-04", "RC53-05", "RC53-08"], "pack_alias": "PACK-CYCLE-03", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-03** (live `BPR-1145`, allocated at import).

# BPR-1145: Persist cycle history, drafts, pins and operation receipts

## Outcome and bounded work

Add additive cycle catalog/draft/round/response storage through the existing SQLite/application ports and immutable content-store references. Implement optimistic revisions, operation deduplication, transactional freeze, pin inventory and explicit archive/removal. Keep old PR schema records unchanged.

## Acceptance and direct evidence

Production-store migration/reopen tests, injected crashes before/after object install/catalog commit, same-request same/different payload, quota pressure with pinned material and deletion impact preview.

Criteria: RC53-04, RC53-05, RC53-08.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
