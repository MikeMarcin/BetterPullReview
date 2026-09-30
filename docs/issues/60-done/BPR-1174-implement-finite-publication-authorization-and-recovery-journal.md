---
schema: "implement-rfc.card.v1"
id: "BPR-1174"
title: "Implement finite publication authorization and recovery journal"
status: "Done"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1172", "BPR-1145"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-06", "RC59-07"], "pack_alias": "PACK-CYCLE-32", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC59-06","outcome":"pass","revision":"da6445e5eae3f8c69290edba6c57d1a15ebe2883","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_publication (real checkout, installed-Git candidate, real store), core_review_cycle_publication and sqlite_review_cycle_storage: preparing records a plan and authorizes nothing; the preview names revisions, host, account, repository, new branch, commit, parent, paths, author/committer, message, PR title/body and the digest; imported approval and consumer grant are not_reviewer, another account authority_changed; edited title/body/account/base/new capture are new digests, untried predecessors superseded and their confirmations refused; only the reviewer's confirmation of the current digest authorizes."}},{"criterion":"RC59-07","outcome":"pass","revision":"da6445e5eae3f8c69290edba6c57d1a15ebe2883","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share: every attempt journaled before it is made, retried operations replay without a second record, a lost answer is uncertain and blocks blind retry and reauthorization until read, a restart with an attempt in flight becomes uncertain with the authorization spent, read-first recovery never resumes writes without a fresh confirmation, cancellation keeps confirmed effects and claims nothing undone; mutation keeping authorization across restart fails two suites. Provider reads, PR association and live cases are BPR-1175/1176/1178's shares."}},{"criterion":"BPR-1174:boundary","outcome":"pass","revision":"da6445e5eae3f8c69290edba6c57d1a15ebe2883","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"103 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only BPR-1211's test_scoped_writing_agent fails), including the schema 1-6 migrations to schema 7, architecture_boundaries and test_network_isolation_manifest with the new pin; clang-format and clang-tidy clean. Linux and Windows runs owed with the selectors on the card."}}]
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

## Owed platform runs

Passed on macOS arm64 at `da6445e5e`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_review_cycle_publication$|^sqlite_review_cycle_storage$|^review_cycle_publication$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` (measure the new `review_cycle_publication` pin contained).

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), `build/dev` freshly configured at `e9e65eab2`, `/usr/bin/git`. Everything ran locally against the fixtures, with no live GitHub.

`core_review_cycle_publication`, `review_cycle_publication` and `sqlite_review_cycle_storage` pass.

The build is clean under GCC with no warnings, and no source change was needed. The logs are station records in `build/logs/bpr-station-publication/` and are not committed.
