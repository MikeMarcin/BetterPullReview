---
schema: "implement-rfc.card.v1"
id: "BPR-1145"
title: "Persist cycle history, drafts, pins and operation receipts"
status: "Done"
rfcs: ["RFC-BPR-0053"]
depends_on: ["BPR-1144"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "domain", "acceptance": ["RC53-04", "RC53-05", "RC53-08"], "pack_alias": "PACK-CYCLE-03", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC53-04","outcome":"pass","revision":"d091a960c49f8c0ac3b57a189d8dc2364cdac140","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"sqlite_review_cycle_storage: forked writer SIGKILLed at COMMIT before and after completion for object install, capture and freeze; reopen shows whole record or none, never a partial round; the same operation then completes once or replays. Missing/corrupt objects reported as object_unavailable/corrupt, never substituted. macOS arm64; Linux/Windows runs owed (card section)."}},{"criterion":"RC53-05","outcome":"pass","revision":"d091a960c49f8c0ac3b57a189d8dc2364cdac140","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Stale expected cycle and draft revisions refused with current state; identical retries replay the stored receipt for create, capture and freeze; reusing an operation id for a different request is refused; refused requests store no receipt. Cross-interface use of the same store is BPR-1146/owner-channel work."}},{"criterion":"RC53-08","outcome":"pass","revision":"d091a960c49f8c0ac3b57a189d8dc2364cdac140","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Budgeted store refuses an over-budget install with the inventory (pinned 40, unreferenced 20); reclaim deletes only unreferenced objects; pinned manifest stays readable; removal requires a current preview token, releases only the cycle's own bytes and never reissues its identity. Publication pins are future RFC-0059 work."}},{"criterion":"RC53-01","outcome":"pass","revision":"d091a960c49f8c0ac3b57a189d8dc2364cdac140","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Reopen share: a provider-free cycle with an empty complete R0 reopens through a new store object with the same catalog incarnation and projects InitialEmpty."}},{"criterion":"RC53-02","outcome":"pass","revision":"d091a960c49f8c0ac3b57a189d8dc2364cdac140","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Persist share: cycle, revision, draft and round identities and all seven counters persist and reload; the removed cycle id is not reissued; no provider id column exists."}},{"criterion":"BPR-1145:boundary","outcome":"pass","revision":"d091a960c49f8c0ac3b57a189d8dc2364cdac140","evidence":{"kind":"command","detail":"sqlite_release_maintenance_files passes with review-cycles.sqlite3 purged; sqlite_review_tour_storage, architecture_boundaries, architecture_boundary_contract, tidy and clang-format pass; documentation gate DOCUMENTATION_OK."}}]
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

## Owed platform evidence

The SQLite contract, including its SIGKILL crash course, ran on macOS arm64
only. Owed, not gating this card: a Linux station runs
`cmake --build --preset dev --target test_review_cycle_storage test_release_maintenance_files`
then `ctest --test-dir build/dev -R "^sqlite_review_cycle_storage$|^sqlite_release_maintenance_files$" --output-on-failure`;
a Windows station runs the same build and
`ctest --test-dir build\dev -R "^sqlite_review_cycle_storage$|^sqlite_release_maintenance_files$" --output-on-failure`
(the crash course is compiled out there, as in the other SQLite contracts).
Record results in the [execution ledger](../../verification/RFC-BPR-0053-0061.md).
