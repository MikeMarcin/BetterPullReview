---
schema: "implement-rfc.card.v1"
id: "BPR-1196"
title: "First tab keeps the selected comparison"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0052/BPR-1132 integration regression", "mandate":"owner-20260925-unattended-parallel/cache-identity"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0a0e3728c03b055f55588009d67018c59c92c529","evidence":{"kind":"observation","detail":"Focused two-head public first-tab regression fails before repair (29.74 s), succeeds on committed source (26.92 s); accepted/window/tab/displayed preserve exact 1:1 anchors and 20,000-line source."}},{"criterion":"AC2","outcome":"pass","revision":"0a0e3728c03b055f55588009d67018c59c92c529","evidence":{"kind":"observation","detail":"Exact 32-head application_large_review_cache_bytes passes in 355.62 s, with eviction/revisit/held-B supersession/retained-storage/destruction assertions and unchanged 840/900-second bounds."}},{"criterion":"AC3","outcome":"pass","revision":"0a0e3728c03b055f55588009d67018c59c92c529","evidence":{"kind":"observation","detail":"All 12 behavior/architecture/core/host/composition integration rows pass; network manifest passes after building its missing unrelated executable prerequisite. Format, tidy, documentation, scoped board validation pass. Existing owner/native obligations remain pending and Windows/Linux remain deferred."}}]
---

# First tab keeps the selected comparison

The full Mac run at `f6de40b6` timed out in
`application_large_review_cache_bytes` after selecting the first file. The
course accepted epoch 1 after retaining 32 heads, but the selected source did
not satisfy epoch 1 identity before the unchanged 840-second run budget.

## Acceptance

- AC1: A public comparison selection before any tab opens remains the exact
  accepted, window, tab and displayed comparison when the first file opens.
  A focused public course exposes the regression before the product repair.
- AC2: The unchanged cache-byte pressure course passes with exact source,
  eviction, supersession and cleanup evidence; the 840/900-second bounds stay
  unchanged.
- AC3: Focused document-tab and epoch checks, composition source pins and the
  documentation gate pass on the candidate. Required owner/native decisions
  on [[BPR-1135]] remain pending; Windows/Linux runs remain deferred.

## Execution plan

Adopt RFC-0052 m2's authored comparison/tab identity contract as a follow-up
regression slice. Source baseline `f6de40b633ea8b879d3643ad5a259169fd976975`;
isolated branch `codex/bpr1132-cache-identity`. Diagnose all four identities at
selection, repair only the comparison/tab composition boundary, prove a
focused failure control and run the complete cache-byte course, then review,
commit explicit paths and release the writer to the coordinator. The primary
checkout and frozen attended app are outside this writer's scope.

## Diagnosis and candidate

The short two-head public course failed at `f6de40b6` plus the passive evidence
sampler: before opening the file, accepted/window/tab were `1:1 / 2:2 / none`;
after opening, accepted/window/tab/displayed were all `2:2`. It failed in
29.74 seconds with the exact-pair assertion. Updating the empty-tab observer
alone still failed in 28.84 seconds: epoch acceptance had queued its observation
behind the next file gesture. The corrected composition also adopts that
observation synchronously before first-tab creation. The focused course then
passed in 27.35 seconds, rendered the full Read window and drained shutdown.

The product change is confined to the empty-tab observer and first-file
composition; existing active-tab rules and the core's immutable pair identities
are unchanged. The sampler is test-build-only and reads state without driving
it. The full application and production-manifest pins are resealed after this
source review; the mutation-branch digest is unchanged. The architecture and
workbench specification now describe comparison ownership with no tab open.

## Verification and acceptance

Product candidate `0a0e3728` on macOS 26.6.2 arm64, Qt 6.11.2, Debug,
offscreen. The tests-disabled Release composition was rebuilt separately by
its contract. The final diagnostic/header documentation changes do not alter
the cache course's product behavior; its source and test implementation are
the committed candidate.

- **AC1 passes.** The focused failure control above detects the original
  defect. `application_first_tab_comparison` passes on the committed candidate
  in 26.92 seconds: after first-file selection, accepted/window/tab/displayed
  all remain `1:1`. It verifies exact anchor and blob identities, all 20,000
  source lines on both sides, a 1600×1000 full-window render and shutdown.
- **AC2 passes.** `application_large_review_cache_bytes` passes in 355.62
  seconds. All 32 retained epochs survive. Twenty large selections admit
  70,353,760 bytes in total and peak at 66,839,408 cached bytes, below the
  unchanged 64 MiB capacity. Eviction releases the first diff and index;
  revisit recomputes the exact pair. Eight source observations preserve the
  superseding file B through A's drain. Shutdown and destruction release the
  diff, index, window and documents while preserving retained Git/catalog
  data. The original 840-second course and 900-second CTest bounds remain.
  Identity evidence in the original 32-head setup is
  `accepted=1:1 window=32:32 tab=none` before the gesture and
  `accepted=1:1 window=1:1 tab=1:1 displayed=1:1` afterward.
- **AC3 passes on macOS.** The 13-row integration passed all 12 behavior,
  architecture, core/host and composition rows, including clean production
  composition (227.94 s), exact-single-request workbench composition
  (23.30 s), source-pinned review round trip (11.49 s), and complete tab
  switching/restart/reopen (101.30 s). The network manifest initially lacked
  the unrelated `gh_cli_git_smart_transport` executable in this limited
  worktree build; after building its exact prerequisite, the manifest passes.
  Format, tidy, documentation and scoped board validation pass. Tidy reports
  22 warnings on source lines already present at `f6de40b6`; no rule was
  disabled. No full-suite, Windows, Linux or native accessibility result is
  inferred.

Commands, changed paths, logs, integration instructions and required pending
acceptance are retained in the
[handoff](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/HANDOFF-cache-identity-2026-09-25.md).

Windows/Linux execution remains owner-deferred under [[BPR-1135]]. When
resumed, the affected portable rows are `application_first_tab_comparison`,
`core_document_tabs`, `application_document_tab_host`, `core_epoch_policy`,
`application_epoch_host`, `application_workbench_host`,
`application_document_tabs_restoration_public_path`,
`application_workbench_composition_contract`,
`application_review_round_trip_composition_contract`, and both architecture
boundaries. The cache-byte memory sampler itself remains Mac-only. Existing
owner questions and platform floors are unchanged.
