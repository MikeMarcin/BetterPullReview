---
schema: "implement-rfc.card.v1"
id: "BPR-1149"
title: "Capture scoped quiescent revisions without source writes"
status: "Done"
rfcs: ["RFC-BPR-0054"]
depends_on: ["BPR-1145", "BPR-1148"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "snapshots", "acceptance": ["RC54-02", "RC54-04", "RC54-05"], "pack_alias": "PACK-CYCLE-07", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC54-02","outcome":"pass","revision":"73d474240f9519adc59236a670064f7e469d4d03","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"macOS arm64 review_cycle_capture: a Git checkout with hooks for every relevant event, core.fsmonitor, filter clean/smudge/process and diff textconv drivers pointing at a canary, '* filter=evil' attributes and a promisor remote is captured with dirty, staged-only, untracked and ignored content; the whole tree including .git (index, refs, config, objects) hashes identically before and after and the canary never runs. Git is never invoked by the product: the index is parsed as data. Working bytes are captured; the staged object id equals git rev-parse :src/b.txt."}},{"criterion":"RC54-04","outcome":"pass","revision":"73d474240f9519adc59236a670064f7e469d4d03","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_capture: one change between passes is retried once and the stable bytes recorded; a writer that never stops exhausts exactly three retries and reports unstable naming the path, recording nothing; a file appearing mid-capture is caught by re-enumeration; only observed_stable is produced (immutable_source reserved, never produced). A mutation that skipped the pass comparison failed the test."}},{"criterion":"RC54-05","outcome":"pass","revision":"73d474240f9519adc59236a670064f7e469d4d03","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_capture: narrowed scope reads nothing outside it; scope version 2 makes a newly visible path unknown, not added; tracked removal is an explicit deletion; untracked removal is absent in a complete capture and unknown in a partial one; skip-worktree (sparse) entries are visible, unread, not deleted, never fetched; ignored, default-set and requested exclusions recorded with reasons; a scope path naming nothing is missing_scope."}},{"criterion":"BPR-1149:cancellation","outcome":"pass","revision":"73d474240f9519adc59236a670064f7e469d4d03","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"review_cycle_capture: cancellation mid-capture records no revision; installed objects are unreferenced and reclaim removes only them, leaving pinned objects intact."}},{"criterion":"BPR-1149:boundary","outcome":"pass","revision":"73d474240f9519adc59236a670064f7e469d4d03","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"ctest -R '^core_review_cycle|^review_cycle_capture$|^workspace_source$|^sqlite_review_cycle|^architecture_boundar|^conditional_branch|network_isolation' passes on macOS arm64; core tests pass under core-only; clang-tidy --warnings-as-errors and clang-format clean on touched units; DOCUMENTATION_OK. Linux and Windows runs owed with the commands in the ledger."}}]
---

Pack alias: **PACK-CYCLE-07** (live `BPR-1149`, allocated at import).

# BPR-1149: Capture scoped quiescent revisions without source writes

## Outcome and bounded work

Implement bounded enumeration, raw-byte capture, before/after checks, finite instability retry and staged immutable manifests. Preserve exclusions, explicit deletions, index/working-tree distinction and observed_stable/immutable_source quality. Never infer global atomicity from two scans.

## Acceptance and direct evidence

Hash source/index/refs before and after capture; hostile hook/filter/lazy-fetch canaries; concurrent writer and enumeration changes; scope extension and partial deletion cases; cancellation with no visible partial revision.

Criteria: RC54-02, RC54-04, RC54-05.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror`, `build/dev` freshly configured at `467254573`, and the fix `5303ab394`. The source volume is ext4 and `/tmp` is tmpfs.

At `467254573` `review_cycle_capture` did not build with GCC: `-Wdangling-reference` read `entry(manifest, "path")` in `cycle_course.hpp` as possibly returning into the temporary `std::string`, which `-Werror` refuses (four call sites). Apple clang does not warn. The reference is into the manifest. `5303ab394` makes the helper take `std::string_view`, as `core::find_entry` already does, and changes nothing else. After it, `review_cycle_capture`, `workspace_source` and `core_review_cycle` pass. The hostile hooks, filters, textconv, fsmonitor and promisor rows, and the scope, exclusion, sparse and deletion rows, ran natively; none of the hostile configuration ran and the tree bytes were unchanged. The retry and cancellation rows also pass.

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
