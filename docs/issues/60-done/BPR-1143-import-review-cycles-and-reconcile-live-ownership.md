---
schema: "implement-rfc.card.v1"
id: "BPR-1143"
title: "Import Review Cycles and reconcile live ownership"
status: "Done"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0054", "RFC-BPR-0055", "RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0058", "RFC-BPR-0059", "RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: []
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "intake", "acceptance": ["RC53-10"], "pack_alias": "PACK-CYCLE-01", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"intake:exit:1","outcome":"pass","revision":"03898d7da9d3cd9a16544ffa8a480237393ec730","evidence":{"kind":"artifact","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"At execution base 22d184166: retained archive validator passes (114 files, 9 RFCs, 52 cards, 11 milestones, 90 rows); manifest and source plan hashes equal the import record; 52 aliases map to 52 distinct IDs equal to reservation BPR-1143..1194, each card exists once with matching pack_alias; one closeout per RFC; native plan check valid with 11 groups and 52 work items. Source drift: 11/30 baseline files match, 19 differ, none missing (intake-audit JSON)."}},{"criterion":"intake:exit:2","outcome":"pass","revision":"03898d7da9d3cd9a16544ffa8a480237393ec730","evidence":{"kind":"observation","detail":"Owned row RC53-10 has direct intake-share evidence; the other 89 rows remain Not run and are owned by later cards. No fabricated product pass."}},{"criterion":"RC53-10","outcome":"pass","revision":"03898d7da9d3cd9a16544ffa8a480237393ec730","evidence":{"kind":"artifact","path":"docs/architecture/product-contract.md","detail":"Intake share only: product contract and threat model (Review Cycles registration section) record the proposed narrow local scope, the single new-PR publication surface and unchanged GitHub authority. BPR-1147 and BPR-1185 own the remaining RC53-10 share; RFC closeout BPR-1186 judges the whole criterion."}},{"criterion":"BPR-1143:direct","outcome":"pass","revision":"03898d7da9d3cd9a16544ffa8a480237393ec730","evidence":{"kind":"command","detail":"verify_pack.py PASS; milestone_plan.py check structural valid; milestone_checkpoint.py bind/check continue-context; issue_board.py validate valid; verify_documentation.py DOCUMENTATION_OK. Landed predecessors and live seams recorded in the ledger; RFC-0023 now Auto-Verified, RFC-0044/0045 owner channel exists."}}]
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
