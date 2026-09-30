---
schema: "implement-rfc.card.v1"
id: "BPR-1173"
title: "Build isolated squashed candidates with exact transformation limits"
status: "Done"
rfcs: ["RFC-BPR-0059"]
depends_on: ["BPR-1172", "BPR-1150"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "publication", "acceptance": ["RC59-02", "RC59-04"], "pack_alias": "PACK-CYCLE-31", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC59-02","outcome":"pass","revision":"b94e2dd026c1c0d28d434da0ffd99caf4f7da570","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_candidate (real SHA-1 and SHA-256 checkouts, installed-Git publication catalog): the whole checkout including .git hashed before and after limited, native and refused builds is unchanged; fsmonitor, hooksPath, hook, required filter, diff.external, gpg.program, pager, sshCommand, alternateRefsCommand and packObjectsHook canaries never fire (positive control fires under ordinary Git); the candidate exists only in the private store and an independent clone reads the commit back byte for byte with P as parent. Mutation using the checkout's repository as the store fails the course."}},{"criterion":"RC59-04","outcome":"pass","revision":"b94e2dd026c1c0d28d434da0ffd99caf4f7da570","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Named CRLF-to-LF normalization previewed with both blobs and read back as LF; binary NUL bytes, executable bit and symlink target bytes exact; a changed LFS path is refused natively as filter_managed and P's pointer kept when excluded; external preparation's patch applies at P with ordinary git apply --cached --binary and names the path for LFS tooling; required signing refused, not committed unsigned; SHA-1 and SHA-256 candidates build and read back. Provider admission of object formats is BPR-1175's share."}},{"criterion":"BPR-1173:boundary","outcome":"pass","revision":"b94e2dd026c1c0d28d434da0ffd99caf4f7da570","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"52 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only BPR-1211's test_scoped_writing_agent fails), including git_process_contract, git_cache_contract, architecture_boundaries and test_network_isolation_manifest with the new pin; clang-format clean, clang-tidy clean on the new units and new adapter lines. Linux and Windows runs owed with the selectors on the card."}}]
---

Pack alias: **PACK-CYCLE-31** (live `BPR-1173`, allocated at import).

# BPR-1173: Build isolated squashed candidates with exact transformation limits

## Outcome and bounded work

Construct a one-parent outgoing tree/commit in a private object store/index using controlled local plumbing. Bind author/committer metadata and transformations. Detect unsupported LFS/filter/signing/object-format cases; keep source checkout untouched.

## Acceptance and direct evidence

Hash user index/refs/files before/after, malicious config/hook/filter canaries, LF transform preview, binary/mode/symlink cases, LFS pointer versus bytes, SHA-1/SHA-256 admission and external-preparation output.

Criteria: RC59-02, RC59-04.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `b94e2dd02`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_review_cycle_candidate$|^review_cycle_candidate$|^review_cycle_outgoing$|^git_process_contract$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` (Git for Windows' `sh` runs the canaries and the positive control; measure the new `review_cycle_candidate` pin contained).

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug), `build/dev` freshly configured at `e9e65eab2`, `/usr/bin/git`. Everything ran locally against the fixtures, with no live GitHub.

`core_review_cycle_candidate`, `review_cycle_candidate`, `review_cycle_outgoing` and `git_process_contract` pass.

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
