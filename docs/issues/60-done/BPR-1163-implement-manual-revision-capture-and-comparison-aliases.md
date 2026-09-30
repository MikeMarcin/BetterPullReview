---
schema: "implement-rfc.card.v1"
id: "BPR-1163"
title: "Implement manual revision capture and comparison aliases"
status: "Done"
rfcs: ["RFC-BPR-0057"]
depends_on: ["BPR-1151", "BPR-1152", "BPR-1158"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "manual-iteration", "acceptance": ["RC57-01", "RC57-02", "RC57-09"], "pack_alias": "PACK-CYCLE-21", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
acceptance: [{"criterion":"RC57-01","outcome":"pass","revision":"48ba9741dbe63a1c2101eeb846a2fdb0c969c048","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"This card's share, macOS arm64 review_cycle_workbench (real folder, store and Git) and application_review_cycle_desktop_public_path: R0 on open, F1 frozen by the runtime with no host running, the folder edited and committed with ordinary tools, nothing sent back; reopening captures nothing, Capture revision records R1 without writing the folder, an unchanged capture records nothing and says so, a deliberate capture records another observation, a branch switch and edit are R3; no receipt, response, agent, commit check or callback is read. The first-value native course is BPR-1166's."}},{"criterion":"RC57-02","outcome":"pass","revision":"48ba9741dbe63a1c2101eeb846a2fdb0c969c048","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"core_review_cycle_views, review_cycle_workbench, qt_review_cycle_view_* and the desktop path: This round (bound start to latest, and with a named result), Since my last review (refused until an explicit checkpoint), All cycle changes and chosen pairs resolve the intended retained material with shared-engine rows; pinned views signal newer material, refuse refresh and survive restart exactly; semantic views and full source move only on refresh; an uncaptured reported result is unavailable; a superseded capture changes nothing. Mutation letting a pinned pair refresh fails the core course."}},{"criterion":"RC57-09","outcome":"pass","revision":"48ba9741dbe63a1c2101eeb846a2fdb0c969c048","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"History entries keep local capture order, the manifest-recorded checkout head (head_moved with unobserved commits, same_head, branch_changed naming refs/heads/feature, no_repository) and imported sender revisions apart; missing captures and parent gaps are explicit; ProviderChronology is always false. Real Git in review_cycle_workbench; the core course covers detached heads, pages and imports."}},{"criterion":"BPR-1163:boundary","outcome":"pass","revision":"48ba9741dbe63a1c2101eeb846a2fdb0c969c048","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"92 focused ctest rows pass on macOS arm64 after a full -k 0 dev build (only the pre-existing test_scoped_writing_agent fails, BPR-1211), including architecture_boundaries and test_network_isolation_manifest; clang-format clean; no new clang-tidy finding class. Linux and Windows runs owed with the selectors on the card."}}]
---

Pack alias: **PACK-CYCLE-21** (live `BPR-1163`, allocated at import).

# BPR-1163: Implement manual revision capture and comparison aliases

## Outcome and bounded work

Add Capture revision as the independent return action. Resolve Full source, This round, Since my last review, All cycle and explicit pairs using actual observations. Preserve source DAG/quality and explicit tab pins; no callback gate or auto-capture watcher.

## Acceptance and direct evidence

R0/F1/R1/F2/R2 manual course with no receipt/report, same-source discussion round, no-change capture, branch divergence, missed revisions, semantic alias versus pinned pair and late completion isolation.

Criteria: RC57-01, RC57-02, RC57-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Owed platform runs

Passed on macOS arm64 at `48ba9741d`. Owed on the Linux station (GCC 15, `build/dev`):
`ctest --test-dir build/dev -R "^core_review_cycle_(views|tabs|comparison)$|^sqlite_review_cycle_storage$|^review_cycle_workbench$|^qt_review_cycle_view_|^application_review_cycle_desktop_public_path$" --output-on-failure`.
Owed on the Windows station (MSVC, `build\dev`): the same selector plus
`|^test_network_isolation_manifest$` (the `review_cycle_workbench` row now takes `--git`).

## Linux — bpr-main-linux, 2026-09-29

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2 with the dev preset's `-Werror` (Debug, where libstdc++ checks its assertions), Qt 6.11.2 offscreen, `build/dev` freshly configured at `52f28dd1b`, and the fix `f4fb1d5bc`.

`core_review_cycle_views`, `core_review_cycle_tabs`, `core_review_cycle_comparison`, `sqlite_review_cycle_storage`, `review_cycle_workbench`, `qt_review_cycle_view_1_0` and `_2_0`, and `application_review_cycle_desktop_public_path` pass. The build of these targets is clean under GCC.

The logs are station records in `build/logs/bpr-station-iteration/` and are not committed.

## Windows — bpr-main-win, 2026-09-29

At `52f28dd1`, `build/windows-dev` (MSVC 19.51) builds with no warning and no
source change. The manual-iteration rows pass offscreen: the six
`core_review_cycle` rows, `sqlite_review_cycle_storage`,
`review_cycle_{workbench,response,verification}`, `qt_review_cycle_view_*`
and `application_review_cycle_desktop_public_path`. The two new uncontained
pins are measured and confirmed in `fb9196d8`. The details are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under
"Windows station, manual iteration".
