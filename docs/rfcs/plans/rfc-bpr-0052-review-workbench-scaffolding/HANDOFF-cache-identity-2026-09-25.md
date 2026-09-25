# BPR-1196 first-tab comparison handoff — 2026-09-25

## Status and assignment

Role: rfc-implementer | Model: gpt-6-astra | Effort: xhigh

Assigned leaf `/root/bpr1132_cache_identity`, continuing the owner's
2026-09-25 unattended/parallel mandate with Codex only. Scope: diagnose and
repair the original full Mac Test 351 cache-byte first-file timeout at main
`f6de40b633ea8b879d3643ad5a259169fd976975`; preserve the 840/900-second bounds.
The coordinator reserved [[BPR-1196]] because [[BPR-1132]] is terminal Done.
The original m2/m3 plans, owner questions and deferred Windows/Linux obligations
remain in force. This is one regression slice, not m2/m3 or RFC acceptance.

Source/tracker root:
`/Users/jaeger/.codex/worktrees/bpr1132-cache-identity/BetterPullReview`, branch
`codex/bpr1132-cache-identity`. Product commit `0a0e3728`; plan `08110dd9`;
claim `1ea0ca99`. The card finish and this documentation are source-identical
descendants. No push or primary-checkout mutation is authorized to this leaf.

## Result and verification

An empty tab collection retained the previous window comparison. Epoch
acceptance also queued an observation behind the next synchronous first-file
gesture. The observer now updates the window pair without an active tab, and
the first-file path adopts the accepted observation before opening a tab.
The passive fixture sampler exposes exact accepted/window/tab pairs; the
public benchmark independently verifies the displayed anchors and blob bytes.
The core's active-tab and stale-completion policies are unchanged.

The pre-repair two-head control failed in 29.74 s: before the first file,
accepted/window/tab were `1:1 / 2:2 / none`; afterward every owner and the
displayed result were `2:2`. Observer-only correction still failed in 28.84 s,
which demonstrated the queued-observation race. The complete correction passed
in 27.35 s; the committed candidate passed in 26.92 s.

Exact cache-byte course: **pass, 355.62 s**. Before selection it observed
accepted `1:1`, window `32:32`, no tab; afterward accepted/window/tab/displayed
were all `1:1`. Twenty pressure selections, exact eviction/revisit, held-B
supersession, all 32 retained Git/catalog pairs and shutdown/destruction checks
pass. Peak cache 66,839,408 bytes; admitted 70,353,760 bytes. Neither cache
capacity nor the course/test timeouts changed.

The integration selector below passed its 12 behavior/architecture rows in a
400.60 s run. Its network-manifest row initially failed because the targeted
build lacked the unrelated `gh_cli_git_smart_transport` binary; building that
exact prerequisite made the unchanged manifest check pass. Clean tests-disabled
Release composition passes in 227.94 s; tab switch/restart/reopen in 101.30 s.
Format and tidy pass; all 22 tidy-warning source lines predate this slice.
Documentation and scoped board validation pass.

```sh
cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache -DBPR_DEPENDENCY_OFFLINE=ON
cmake --build --preset dev -j6 --target bench_better_pull_review_large_review app_better_pull_review test_better_pull_review_document_tabs_core_tests test_better_pull_review_document_tab_host_tests test_better_pull_review_epoch_core_tests test_better_pull_review_application_epoch_host_tests test_better_pull_review_application_workbench_host_tests test_profile_window_review_restoration_public_path
ctest --test-dir build/dev -j1 --output-on-failure -R '^application_large_review_cache_bytes$'
ctest --test-dir build/dev -j1 --output-on-failure -R '^(application_first_tab_comparison|core_document_tabs|application_document_tab_host|core_epoch_policy|application_epoch_host|application_workbench_host|application_document_tabs_restoration_public_path|application_workbench_composition_contract|application_review_round_trip_composition_contract|application_composition_contract|architecture_boundaries|core_dependency_boundary|test_network_isolation_manifest)$'
cmake --build --preset dev -j2 --target test_better_pull_review_gh_cli_git_smart_transport_tests
ctest --test-dir build/dev -j1 --output-on-failure -R '^test_network_isolation_manifest$'
python3 _coding-standard/scripts/format.py --dry-run src/app/src/github_application.cpp src/app/src/github_application_fixture_access.hpp tests/end_to_end/large_review_benchmark.cpp
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths src/app/src/github_application.cpp tests/end_to_end/large_review_benchmark.cpp
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=/Users/jaeger/.agents/skills/implement-rfc/scripts python3 scripts/verify_documentation.py --root .
PYTHONDONTWRITEBYTECODE=1 python3 /Users/jaeger/.agents/skills/implement-rfc/scripts/issue_board.py --repo-root /Users/jaeger/.codex/worktrees/bpr1132-cache-identity/BetterPullReview validate --include BPR-1196 --json
```

Local evidence: `/Users/jaeger/.codex/tmp/bpr1196-control.log`,
`bpr1196-fixed.log`, `bpr1196-fixed2.log`, `bpr1196-cache-bytes.log`,
`bpr1196-cache-bytes-detail.log`, `bpr1196-cache-bytes.json`,
`bpr1196-integration.log`, `bpr1196-integration-detail.log`,
`bpr1196-manifest-rerun.log`, `bpr1196-tidy.log` and the build logs under the
same prefix. Renders and final JSON are under the isolated
`build/dev/tests/end_to_end/`. They contain only the synthetic fixture.

## Changed paths and source review

- `src/app/src/github_application.cpp`: empty-tab observation and first-file
  synchronization; passive test-only identity sampler.
- `src/app/src/github_application_fixture_access.hpp`: the sampler's typed
  observation contract, absent from tests-disabled production.
- `tests/end_to_end/large_review_benchmark.cpp`: short two-head public
  regression, exact identity/source assertions and cache-course diagnostics.
- `tests/end_to_end/CMakeLists.txt`: the isolated short regression row.
- `tests/end_to_end/application_review_round_trip_composition_contract.cmake`:
  reviewed full-source/production-manifest reseals; mutation digest unchanged.
- `docs/architecture/system-architecture.md`,
  `docs/ux/workbench-specification.md`: empty-tab comparison ownership.
- [[BPR-1196]] and this handoff: plan, failure history, evidence and acceptance.

The source review covers all product changes from `f6de40b6`. No new provider,
credential, transport, mutation authority, storage capability or retry is
introduced. Full source digest
`e8382d339800dbe586053eaec61f0c1467860f801cc9c5da49dafaa0a8d2c087`;
production manifest
`1a7bf2a2997a5b9c00aceacfa8994a8bd152563a45ee892dd31f130230d50c42`.
The coordinator must review/reseal if integrating other application-source
changes alongside this commit.

## Review, ownership and continuation

No additional attended session, native observation or review server was
created. The existing frozen m2 candidate, fixtures, Qt installation and
coordinator-owned server/watch were not modified. All owner questions,
VoiceOver/Narrator and Windows/Linux platform evidence remain pending as the
[m2 handoff](HANDOFF-m2-2026-09-25.md) records. Windows/Linux execution stays
deferred; no platform floor was lowered.

All task-owned build/test/tidy commands finish before writer release. The
isolated worktree and synthetic evidence remain for integration; there is no
background child or task-owned server to retain. No agents were spawned.

Next action: after the leaf's final writer-release report, cherry-pick plan,
claim, product and finish commits. **Skip duplicate reservation imports**
`7849fb5b` (= main `d50f2f03`), `ebf61353` (= `cbf42935`) and `32dfb052`
(= `480af68d`). Reconcile source pins with the other independently owned Mac
regression slices, then run the affected combined-candidate checks. Continue
the original RFC acceptance only after its actual owner/native exits hold.
