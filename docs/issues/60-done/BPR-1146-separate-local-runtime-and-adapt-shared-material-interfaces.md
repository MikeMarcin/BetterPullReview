---
schema: "implement-rfc.card.v1"
id: "BPR-1146"
title: "Separate local runtime and adapt shared material interfaces"
status: "Done"
rfcs: ["RFC-BPR-0053", "RFC-BPR-0060"]
depends_on: ["BPR-1144"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "domain", "acceptance": ["RC53-03", "RC53-06", "RC53-07"], "pack_alias": "PACK-CYCLE-04", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC53-03","outcome":"pass","revision":"16e35c4ac34413739a14d790a77e014a3188e6be","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"sqlite_review_cycle_runtime: review_material_bridge routes pull-request diff_blob_request unchanged to the existing diff_blob_port and cycle requests only to this catalog's store for a revision of the named cycle and the object it recorded; foreign catalog, unrecorded object and PR-without-port are refused, no cross-fallback. Cycle material is bounded by MaximumBytes. No PR authority or epoch type changed."}},{"criterion":"RC53-06","outcome":"pass","revision":"16e35c4ac34413739a14d790a77e014a3188e6be","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_runtime opens, creates, captures an empty R0 and reopens with only a profile root and two executors; review_cycle_dependency_closure computes its link closure (adapter_sqlite, application, sqlite3, platform, core, Threads) with no agent, MCP, AI, GitHub, Git, Qt or composition target; a closure containing adapter_agent fails the check. Remaining packaging couplings recorded for BPR-1179."}},{"criterion":"RC53-07","outcome":"pass","revision":"16e35c4ac34413739a14d790a77e014a3188e6be","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"On a profile root holding a pull-request tour draft, the cycle runtime creates its own catalog; review-tours.sqlite3 bytes are identical afterwards, the draft reads back exactly, and the cycle list holds only the created cycle. BPR-1145 showed a neighbouring database unchanged and profile purge owning the new file."}},{"criterion":"BPR-1146:subject-switch","outcome":"pass","revision":"16e35c4ac34413739a14d790a77e014a3188e6be","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Work queued for a cycle selection completes as cancelled after switching to a pull-request subject; pull-request selections are refused for load and capture; switching back reads the untouched cycle."}},{"criterion":"BPR-1146:boundary","outcome":"pass","revision":"16e35c4ac34413739a14d790a77e014a3188e6be","evidence":{"kind":"command","detail":"Integration pass at this revision: ctest -R '^sqlite_|^core_|^architecture_boundar|^test_network_isolation_manifest$|^review_cycle|^live_mutation_isolation_contract$' 77/77 passed; full dev build fails only on the two pre-existing toolchain failures recorded on BPR-1211/BPR-1214 (test_scoped_writing_agent as C++98, agent_profile_tests size assertion); tidy and clang-format pass."}}]
---

Pack alias: **PACK-CYCLE-04** (live `BPR-1146`, allocated at import).

# BPR-1146: Separate local runtime and adapt shared material interfaces

## Outcome and bounded work

Compose a local profile/runtime path independent of account enrollment and agent discovery. Add the tagged material bridge to the existing workbench/diff APIs. Isolate optional agent dependencies at the application seam early; record remaining packaging couplings for card 37.

## Acceptance and direct evidence

No-agent/no-account initialization, old PR migration and subject-switch tests, cross-authority cache refusal and link/dependency inspection of core cycle services.

Criteria: RC53-03, RC53-06, RC53-07.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.
