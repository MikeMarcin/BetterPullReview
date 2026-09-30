---
schema: "implement-rfc.card.v1"
id: "BPR-1172"
title: "Plan selected outgoing content over an explicit Git base"
status: "Done"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1163", "BPR-1165"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-01", "RC59-03"], "pack_alias": "PACK-CYCLE-30", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC59-01","outcome":"pass","revision":"b7fc9328fd1f4008ec84de01ab6854ec8b1b369d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_outgoing (real SHA-1 and SHA-256 checkouts, real store, installed-Git publication catalog) and core_review_cycle_outgoing: R0, Rn, P (exact commit HEAD names) and T (refs/remotes/origin/main, moved) stay separate; dirty starting work under an improvement goes out whole, unrelated untracked scratch stays out, a draft a note names is relevant, improvements-only asks before an improvement over omitted starting work; T's drift on a selected path and its local-reference source are named in the readable preview; checkout and .git byte-for-byte unchanged. The window's preview is BPR-1177's."}},{"criterion":"RC59-03","outcome":"pass","revision":"b7fc9328fd1f4008ec84de01ab6854ec8b1b369d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"The candidate is P's complete listing with only selected rows changed: module-only and excluded-folder captures keep P's other entries, a path only a later scope saw is not proposed, a narrowed final capture keeps P's, deletion/rename/mode change explicit, file/folder collision blocks the build, a candidate equal to P is No change, a plain folder is repository_required with no synthetic root commit; P's listing is admitted only when the commit hashes to P and the listing rebuilds P's tree. Mutation treating out-of-scope as absent fails both courses."}},{"criterion":"BPR-1172:boundary","outcome":"pass","revision":"b7fc9328fd1f4008ec84de01ab6854ec8b1b369d","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"51 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only BPR-1211's test_scoped_writing_agent fails), including git_process_contract, git_cache_contract, architecture_boundaries and test_network_isolation_manifest with the new pin; clang-format clean, clang-tidy clean on new units. Linux and Windows runs owed with the selectors on the card."}}]
---

Pack alias: **PACK-CYCLE-30** (live `BPR-1172`, allocated at import).

# BPR-1172: Plan selected outgoing content over an explicit Git base

## Outcome and bounded work

Implement R0/Rn/P/T selection, preexisting-work versus cycle-improvement inventory and candidate preview. Start from the complete P tree and overlay explicit selected changes; preserve uncaptured paths and identify dependency/conflict gaps.

## Acceptance and direct evidence

Dirty starting work, unrelated edits, module-only scope, scope extension, deletion/rename/mode change, empty candidate, moved target and iteration-only patch depending on omitted initial changes.

Criteria: RC59-01, RC59-03.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `b7fc9328f`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_git_objects$|^core_review_cycle_outgoing$|^review_cycle_outgoing$|^git_process_contract$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` (the alternates separator is `;`; measure the new `review_cycle_outgoing` pin contained).

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), `build/dev` freshly configured at `e9e65eab2`, `/usr/bin/git`. Everything ran locally against the fixtures, with no live GitHub.

`core_git_objects`, `core_review_cycle_outgoing`, `review_cycle_outgoing` and `git_process_contract` pass.

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
