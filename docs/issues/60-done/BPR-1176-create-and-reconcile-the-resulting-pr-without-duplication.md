---
schema: "implement-rfc.card.v1"
id: "BPR-1176"
title: "Create and reconcile the resulting PR without duplication"
status: "Done"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1175"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-05", "RC59-07", "RC59-10"], "pack_alias": "PACK-CYCLE-34", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC59-05","outcome":"pass","revision":"fab189080207f43aec407eb1af4169be70b91b31","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share (the pull request): native_https_transport_fixture publication-course reads PRs for the exact head/base, creates one and reads it again through the production transport; review_cycle_publisher creates it only after the branch is confirmed, with the confirmed text and draft choice, and never again once linked. Objects/branch share is BPR-1175's. Live GitHub.com evidence added at 4ce2d0529 (RC59-10 course on MikeMarcin/BPR-Test)."}},{"criterion":"RC59-07","outcome":"pass","revision":"fab189080207f43aec407eb1af4169be70b91b31","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share, macOS arm64 review_cycle_publisher: lost create answer plus restart recovered as the one PR of ours, never recreated; two candidates stay uncertain; a foreign PR opened just before ours is recorded foreign with no duplicate; an existing PR stops the step with no write; 403 fails; a push to the head after creation is recorded as drift; a restart between intent and attempt reads nothing and creates once after fresh confirmation. Mutation adopting one-of-several fails the course. Full fault matrix is BPR-1178's. Live GitHub.com evidence added at 4ce2d0529 (RC59-10 course on MikeMarcin/BPR-Test)."}},{"criterion":"RC59-10","outcome":"pass","revision":"4ce2d0529","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"GitHub.com, the one host native publication claims: run_live_publication_course.py (stamp 20260930T102712Z) on owner-designated MikeMarcin/BPR-Test through the production publisher, session and native HTTPS transport over product source 634e8ac6c. End to end with server identities equal to local (offsets +0530/-0700 kept, null-identity deletion), ready #12 and draft #13, lost branch answer recovered (#14), lost PR answer recovered after restart (#15), foreign branch refused with zero writes, raced branch and raced PR (#16) recorded foreign with nothing overwritten or duplicated; confirmed by git fetch and gh api. GHES stays unadmitted (no designated target). Linux/Windows station runs owed, not gating."}},{"criterion":"BPR-1176:boundary","outcome":"pass","revision":"fab189080207f43aec407eb1af4169be70b91b31","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"133 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only BPR-1211's test_scoped_writing_agent fails); clang-format and clang-tidy clean. Linux and Windows runs owed with the selectors on the card."}}]
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

## Blocker (lifted 2026-09-30): no safe GitHub publication target

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

## Owner designation — 2026-09-30

Asked "BPR-1176 stays blocked until you specify a safe GitHub target", the
owner answered on 2026-09-30: "You can do anything you need to with the
MikeMarcin/BPR-Test github repo". That names github.com
`MikeMarcin/BPR-Test` (public, the `MikeMarcin` account has admin, no
workflow files on any branch) as the disposable target for the RC59-10
course, under the existing `gh` sign-in on the Mac. It designates nothing on
GHES, so GHES stays unadmitted for native publication and its course stays
owed. The `safe-github-publication-target` blocker is lifted for GitHub.com.

## RC59-10 live course — macOS arm64, 2026-09-30

Course `4ce2d0529` over product source `634e8ac6c`:
`tool_review_cycle_live_publication` (built only with
`-DBPR_BUILD_LIVE_PUBLICATION_COURSE=ON`) composes the shipped store,
installed-Git catalog, publication host, `review_cycle_publisher`,
`github_publication_session` and production native HTTPS transport against
`MikeMarcin/BPR-Test` only, and `tests/scripts/run_live_publication_course.py`
prepares the clone and checks the result through `git fetch` and `gh api`.
Every case passes (run `20260930T102712Z`): one plan end to end with a ready
PR (#12), a draft PR (#13), a lost branch answer recovered by reading (#14),
a lost PR answer recovered after a restart (#15), a branch another client
pushed refused with zero writes, and a branch and a PR (#16, another actor's)
taken between the read and the write, both recorded as foreign with nothing
overwritten or duplicated.

The course's two open questions are answered: GitHub's commit endpoint keeps
non-zero offsets exactly (the raw commit reads `+0530` and `-0700`, and the
server named the locally computed commit), and a `"sha": null` tree entry
deletes the path. The product needed no change. The first attempt stopped on
a fault in the course itself, now fixed. Details, the full list of branches
and pull requests left on BPR-Test, and the owed station runs are in the
[ledger](../../verification/RFC-BPR-0053-0061.md), under "BPR-1176: the
RC59-10 live course on MikeMarcin/BPR-Test".

RC59-10 passes for GitHub.com, the only host native publication claims. GHES
stays unadmitted with no designated target; admitting it needs its own
course. The window's Publish as PR gesture is BPR-1177's, and BPR-1178's
share of RC59-10 is not started.

Owed, not gating: the same driver on the Linux and Windows stations with a
`gh` sign-in that can push to BPR-Test (command in the ledger).

## Windows RC59-10 live run: owed (bpr-main-win, 2026-09-30)

Not run. The coordinator relayed the owner's designation of
`MikeMarcin/BPR-Test`. But this station's session was started with "no remote
GitHub mutations", and it takes live mutations only on its own operator's
direct word. So the Windows live course on `MikeMarcin/BPR-Test`, with the
`.exe` tool and Git for Windows as
[live target qualification](../../verification/live-target-qualification-2026-09-30.md)
describes, is still owed. The macOS run at `4ce2d0529` is unaffected.

The Windows prerequisites the course relies on were checked earlier on this
station. They are recorded above:

- `review_cycle_publisher` and `review_cycle_publication` pass.
- `git_process_contract` and `github_publication_contract` pass.
- The `;` alternates separator is right.
- The publication rows' uncontained pins are measured.
