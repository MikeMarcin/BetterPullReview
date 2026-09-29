---
schema: "implement-rfc.card.v1"
id: "BPR-1143"
title: "Import Review Cycles and reconcile live ownership"
status: "Ready"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0054", "RFC-BPR-0055", "RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0058", "RFC-BPR-0059", "RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: []
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "intake", "acceptance": ["RC53-10"], "pack_alias": "PACK-CYCLE-01", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

Pack alias: **PACK-CYCLE-01** (live `BPR-1143`, allocated at import).

# BPR-1143: Import Review Cycles and reconcile live ownership

## Outcome and bounded work

Validate the archive; inspect live HEAD, canonical RFC order, predecessor import maps, the allocator and installed implement-rfc schema. Allocate/remap nine new RFCs and 52 card aliases. Adopt all eleven authored milestones. Record actual source/editor/profile-owner seams and which earlier proposals have landed. Preserve RFC0052 and the original tour/conventions packs; no duplicate import or new implied execution exception.

## Acceptance and direct evidence

Archive hashes, complete RFC/issue remap, returned allocation receipts/counter paths and a native plan-import receipt. Verify every card appears once and source drift is recorded.

Criteria: RC53-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
