---
schema: "implement-rfc.card.v1"
id: "BPR-1176"
title: "Create and reconcile the resulting PR without duplication"
status: "Blocked"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1175"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-05", "RC59-07", "RC59-10"], "pack_alias": "PACK-CYCLE-34", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC59-05","outcome":"pass","revision":"fab189080207f43aec407eb1af4169be70b91b31","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share (the pull request): native_https_transport_fixture publication-course reads PRs for the exact head/base, creates one and reads it again through the production transport; review_cycle_publisher creates it only after the branch is confirmed, with the confirmed text and draft choice, and never again once linked. Objects/branch share is BPR-1175's."}},{"criterion":"RC59-07","outcome":"pass","revision":"fab189080207f43aec407eb1af4169be70b91b31","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share, macOS arm64 review_cycle_publisher: lost create answer plus restart recovered as the one PR of ours, never recreated; two candidates stay uncertain; a foreign PR opened just before ours is recorded foreign with no duplicate; an existing PR stops the step with no write; 403 fails; a push to the head after creation is recorded as drift; a restart between intent and attempt reads nothing and creates once after fresh confirmation. Mutation adopting one-of-several fails the course. Full fault matrix is BPR-1178's."}},{"criterion":"RC59-10","outcome":"pending","revision":"fab189080207f43aec407eb1af4169be70b91b31","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Needs one separately approved disposable GitHub/GHES target course per claimed host. No safe target has been named; nothing was created on GitHub, GHES is not admitted for native publication. Blocked on safe-github-publication-target; fixture and loopback evidence is not live qualification."}},{"criterion":"BPR-1176:boundary","outcome":"pass","revision":"fab189080207f43aec407eb1af4169be70b91b31","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"133 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only BPR-1211's test_scoped_writing_agent fails); clang-format and clang-tidy clean. Linux and Windows runs owed with the selectors on the card."}}]
blocked_by: "safe-github-publication-target"
---

Pack alias: **PACK-CYCLE-34** (live `BPR-1176`, allocated at import).

# BPR-1176: Create and reconcile the resulting PR without duplication

## Outcome and bounded work

Create the PR only after the exact new head is verified. Reconcile matching head/base/repository/account after timeout; distinguish uniquely evidenced creation from coincidental/ambiguous matches. Re-read actual head/base for concurrent drift.

## Acceptance and direct evidence

Create success, lost response, existing foreign PR/branch, multiple matches, base/head movement, capability/permission refusal and restart. One separately approved disposable GitHub/GHES target course per claimed supported host.

Criteria: RC59-05, RC59-07, RC59-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `fab189080`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^review_cycle_publisher$|^native_https_transport_fixture$|^github_publication_contract$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`):
`ctest --test-dir build\dev -R "^review_cycle_publisher$|^github_publication_contract$|^test_network_isolation_manifest$" --output-on-failure`.

## Blocker: no safe GitHub publication target

RC59-10 needs one separately approved disposable GitHub/GHES target course per claimed host. The owner has named no safe target, so no remote branch, commit or pull request has been created and GHES stays unadmitted for native publication. RC59-05 and RC59-07 pass on fixture, simulation and loopback evidence only for this card's share. The owed live course is recorded in the [ledger](../../verification/RFC-BPR-0053-0061.md). Unblock by naming a disposable repository and account for the live course.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), `build/dev` freshly configured at `e9e65eab2`, `/usr/bin/git`. Everything ran locally against the fixtures, with no live GitHub.

Evidence only; the card stays blocked. Its rows, `review_cycle_publisher`, `github_publication_contract` and `native_https_transport_fixture`, pass locally.

The build is clean under GCC with no warnings, and no source change was needed. The logs are station records in `build/logs/bpr-station-publication/` and are not committed.

## Windows — bpr-main-win, 2026-09-30

At `e9e65eab`, with one MSVC rename in the Git adapter, `build/windows-dev`
builds with no warning. The publication rows pass, except
`review_cycle_candidate`, which fails at its symbolic-link case: on Windows a
new link is left out of the candidate and the preview without a word
([[BPR-1244]]). With that case bypassed in a probe, the rest of the course
passes, and the Git for Windows canary is live but never run by the product.
The four new uncontained pins are measured and confirmed. The details are in
the [execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station (publication)".

This entry adds evidence only. The card's status is unchanged.
