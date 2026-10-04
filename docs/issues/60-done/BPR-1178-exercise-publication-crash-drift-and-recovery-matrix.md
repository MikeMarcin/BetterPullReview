---
schema: "implement-rfc.card.v1"
id: "BPR-1178"
title: "Exercise publication crash, drift and recovery matrix"
status: "Done"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1177"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-07", "RC59-10"], "pack_alias": "PACK-CYCLE-36", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC59-07","outcome":"pass","revision":"4260d4bab","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share, macOS arm64 at 4260d4bab: review_cycle_publication_matrix (tests/scripts/review_cycle_publication_matrix_tests.py) passes 22/22. Real processes of the desktop cycle host (real store, checkout, installed-Git catalog) publishing to a file-backed simulated GitHub are SIGKILLed before and after every blob, tree, commit, branch and pull request write and before the post-create read, then restarted: no authorization survives, nothing is sent without a fresh confirmation, recovery only reads, every step is written exactly once, never two open pull requests of ours; drift while down (base moved, branch taken or force-moved, pull raced, ambiguous, head pushed) ends target_moved/target_drift/uncertain/drifted link with nothing overwritten or duplicated; cancel keeps confirmed steps and claims nothing undone. SQLite integrity checked after each kill. Four defects fixed (stuck Publishing after recovery or refusal, 'Nothing was sent' after confirmed steps, own moved-head PR called foreign). Nine negative controls each fail. Live: two in-flight cancellations on MikeMarcin/BPR-Test (run 20261004T151058Z) were taken by GitHub, held uncertain until read, recovered to one branch and one PR each (#39, #40)."}},{"criterion":"RC59-10","outcome":"pass","revision":"4260d4bab","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share: live cancellation cases on the owner-designated GitHub.com target MikeMarcin/BPR-Test through the production publisher, session and native HTTPS transport (run 20261004T151058Z, gh 2.101.0 as MikeMarcin): branch and pull request writes cancelled in flight recorded uncertain while GitHub held them (confirmed remote effects recorded), recovered by reading with no blind retry and no duplicate; the seven earlier live cases pass again (#34-#38) with all 12 git fetch / gh api checks. In the fixture matrix an ambiguous pull request linked by the reviewer is recorded as their own (external) link, and a native pull request whose head moved stays native with drift recorded, not misreported. A live timeout case was not run: the production transport has no timeout seam. GHES stays unadmitted. Linux/Windows station runs owed, not gating."}},{"criterion":"BPR-1178:boundary","outcome":"pass","revision":"4260d4bab","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Full -k 0 dev build on macOS arm64; 56/56 focused rows pass (core_review_cycle_*, review_cycle_*, sqlite_review_cycle_storage, qt_review_cycle_view_*, application_review_cycle_* including the publish public path with a new stop-between-sends step, github_publication_contract, application_review_round_trip_composition_contract, application_github_host, github_provider, native_https_transport_fixture); clang-format and tidy --warnings-as-errors clean on changed units; architecture_boundaries passes on a clean export (fails in the checkout only for another agent's .claude/worktrees). Linux and Windows runs owed with the selectors in the ledger."}}]
---

Pack alias: **PACK-CYCLE-36** (live `BPR-1178`, allocated at import).

# BPR-1178: Exercise publication crash, drift and recovery matrix

## Outcome and bounded work

Inject failure at every object/ref/PR boundary, including after a remote effect but before local receipt. Verify read-first recovery, idempotent association, no force/cleanup fiction and current authorization for remaining operations.

## Acceptance and direct evidence

Fixture matrix plus separately authorized live timeout/cancellation cases where feasible. Record confirmed remote effects and residual races. External fallback success does not pass native publication rows.

Criteria: RC59-07, RC59-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented — macOS, 2026-10-04

Promoted to Ready at `6b06aa67f` under the mandate (BPR-1177 Done), claimed at
`a83caa127`, implemented at `4260d4bab`. `review_cycle_publication_matrix`
runs RC59-07's matrix as real processes: the desktop's cycle host on the real
store, checkout and installed-Git catalog, publishing to a simulated GitHub
whose state is a file, killed at every object, branch and pull request
boundary (before the request leaves, and after GitHub took it but before the
journal recorded it), restarted, with another actor's changes while nothing
ran. 22 cases pass; nine negative controls fail where they should.

It found four defects, fixed at their source: the sheet was stuck Publishing
after a restart and Check GitHub (or a request that never left), with no way to
confirm again or cancel; a refused confirmation left it stuck too; a limit met
after earlier steps said "Nothing was sent"; and recovery called the plan's own
pull request foreign when its head was pushed to while the answer was lost.

Live, on the designated `MikeMarcin/BPR-Test` (run `20261004T151058Z`): a
branch and a pull request write cancelled in flight were both taken by GitHub
and held uncertain until read, then recovered with one branch and one pull
request each (#39, #40); the seven earlier live cases pass again (#34–#38).

Details, the case table, controls, residual races and the owed station runs
are in the [execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"BPR-1178: the publication crash, drift and recovery matrix".

## Owed platform runs

Not gating; the commands are in the ledger section above. The new row is
pinned `dos-volume-name` on Windows by its shape and is owed a contained
measurement there.
