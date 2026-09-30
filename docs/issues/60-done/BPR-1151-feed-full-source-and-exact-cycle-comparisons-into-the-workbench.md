---
schema: "implement-rfc.card.v1"
id: "BPR-1151"
title: "Feed full source and exact cycle comparisons into the workbench"
status: "Done"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0057"]
depends_on: ["BPR-1149", "BPR-1150"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC54-08", "RC54-09", "RC57-02"], "pack_alias": "PACK-CYCLE-09", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC54-08","outcome":"pass","revision":"a70b3ece198428e52b30543cc46cf60f1bf792c8","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64: comparison pages bounded to 256 paths and resumable (core_review_cycle_comparison, review_cycle_comparison), cancellation returns cancelled, reads run on the effect lane with a two-manifest cache; measured with review_cycle_scale --files 100000 (Release, Apple M4 Pro): initial capture 15.5 s, unchanged recapture 8.9 s, 1,000-file change 10.0 s, page p50 0.46/p95 0.55 ms, file diff p50 2.5/p95 3.3 ms, peak RSS 99 MiB. This card's share; native input-latency measurement is BPR-1184's."}},{"criterion":"RC54-09","outcome":"pass","revision":"a70b3ece198428e52b30543cc46cf60f1bf792c8","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_comparison: after the workspace file is edited and the folder deleted, and after restart with a new store object, full source reads the captured bytes; a corrupted chunk reads corrupt and a removed object missing, with no bytes and no diff; the bridge serves only objects the named revision pinned and never reads the workspace. BPR-1152 qualifies recovery."}},{"criterion":"RC57-02","outcome":"pass","revision":"a70b3ece198428e52b30543cc46cf60f1bf792c8","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"core_review_cycle_comparison and review_cycle_comparison: full source resolves to one revision served as exact retained bytes; all-cycle, since-checkpoint and this-round views resolve to the intended pair; explicit pairs and named round results never retarget after newer captures (newer_available only); semantic views move on explicit refresh; late completions for superseded selections are cancelled/dropped while previous content stays. This card's share; the native tab surface is BPR-1163's."}},{"criterion":"BPR-1151:shared-engine","outcome":"pass","revision":"a70b3ece198428e52b30543cc46cf60f1bf792c8","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"compute_material_diff shares the templated alignment and computed-diff rules with compute_file_diff under a cycle-derived material_comparison_key (no pull-request authority); all 52 core tests plus application diff/workbench and Qt diff consumers pass unchanged."}},{"criterion":"BPR-1151:boundary","outcome":"pass","revision":"a70b3ece198428e52b30543cc46cf60f1bf792c8","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"ctest over core, review_cycle*, sqlite_review_cycle*, workspace_source, diff/workbench application and Qt consumers, architecture boundaries and network isolation: 76/76 on macOS arm64; full dev build compiles all targets except pre-existing test_scoped_writing_agent; tidy clean except pre-existing diff_compute primary_index_limit finding; DOCUMENTATION_OK. Linux/Windows runs owed."}}]
---

Pack alias: **PACK-CYCLE-09** (live `BPR-1151`, allocated at import).

# BPR-1151: Feed full source and exact cycle comparisons into the workbench

## Outcome and bounded work

Adapt retained cycle manifests to existing file/material/diff services. Add true single-source observations and exact comparison pairs without fake PR epochs. Page tree/source requests, keep stable previous content and refuse missing/corrupt blobs rather than substituting live paths.

## Acceptance and direct evidence

Core/host tests for R0 full source, R0->R1, content-equivalent paths, missing pins, late material completion, expanded scope and cancellation. Verify real shared diff path, not a new toy comparator.

Criteria: RC54-08, RC54-09, RC57-02.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror`, `build/dev` freshly configured at `467254573`, and the fix `5303ab394`. The source volume is ext4 and `/tmp` is tmpfs.

`core_review_cycle_comparison`, `review_cycle_comparison` and `workspace_source` pass. `review_cycle_scale --files 2000` (the row's default size, in a Debug dev build) passes: initial capture 561 ms, unchanged recapture 364 ms, changed recapture 386 ms, comparison page p50 1.16 / p95 1.27 ms, file diff p50 7.97 / p95 11.5 ms, peak RSS 8.4 MiB. These are observations, not budgets, and are not comparable with the 100,000-file Release macOS figures.

The logs are station records in `build/logs/bpr-station-snapshots/` and are not committed.

## Windows — bpr-main-win, 2026-09-29

At `46725457`, `build/windows-dev` (MSVC 19.51) builds with no warning and
no source change, including the workspace adapter's Win32 branch.
`workspace_source`, `review_cycle_capture`, `review_cycle_comparison`,
`review_cycle_scale`, `review_cycle_recovery`, the six `core_review_cycle`
rows and `sqlite_review_cycle_storage` pass. Contained, the five workspace
rows each fail before their first course completes, so their
`WINDOWS_UNCONTAINED` pins are confirmed. The details are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station, snapshots".
