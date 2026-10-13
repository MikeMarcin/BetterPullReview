---
schema: "implement-rfc.card.v1"
id: "BPR-1436"
title: "Share one QProcess supervision between the gh and Git runners, and bound gh's background reap"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"051a17367","evidence":{"kind":"observation","detail":"better_pull_review_process_support (Qt Core only) holds serialized_process_lane and the group wait, kill, resolution, kill-and-reap, background reap and abandonment. The gh runner (1330 to 1152 lines) and the Git runner (7575 to 7384) use it. The architecture document and AGENTS.md record the shared adapter support target.","path":"docs/issues/60-done/BPR-1436-share-qprocess-supervision.md"}},{"criterion":"AC2","outcome":"pass","revision":"051a17367","evidence":{"kind":"observation","detail":"gh background reap is bounded by github_cli_process_max_reap_timeout, then abandons the child and stays quarantined. github_cli_process_runner test_unreapable_child_is_abandoned failed against the restored unbounded loop (destroying the runner did not return) and passes now.","path":"docs/issues/60-done/BPR-1436-share-qprocess-supervision.md"}},{"criterion":"AC3","outcome":"pass","revision":"051a17367","evidence":{"kind":"observation","detail":"The boundary gate designates the support sources as QProcess owners, refuses other layers, launch, environment and Windows containment surfaces there, and pins the lane's quarantine, watchdog and bounded reap. architecture_boundary_contract passes 152 tests, including test_shared_process_supervision_contract. The descriptor and handle-list owner lists are unchanged.","path":"docs/issues/60-done/BPR-1436-share-qprocess-supervision.md"}},{"criterion":"AC4","outcome":"pass","revision":"051a17367","evidence":{"kind":"observation","detail":"Windows blocks in both runners are textually unchanged, and Git's job terminate is kept through the tree-kill parameter. The gh Job Object and windows_agent_process consolidation are listed as follow-ups for the Windows lane. Nothing was compiled for Windows.","path":"docs/issues/60-done/BPR-1436-share-qprocess-supervision.md"}}]
---

# Share one QProcess supervision between the gh and Git runners, and bound gh's background reap

The 2026-10-13 audit found that the gh runner
([github_cli_process_runner.cpp](../../../src/adapters/github/src/github_cli_process_runner.cpp))
and the installed-Git runner
([installed_git_process.cpp](../../../src/adapters/git/src/installed_git_process.cpp))
each carried their own serialized process worker, process-group kill,
process-group resolution, bounded wait slice, kill-and-reap and background
reap. The two copies had drifted. Git's background reap stops at a deadline,
abandons the child and leaves its worker quarantined with `reap_failure`.
gh's looped with no deadline, so one child that never exits held its worker
thread for good, and destroying the runner joined that thread and never
returned. The owner approved consolidating them on 2026-10-13.

Adapters may not depend on each other, and the platform target is
header-only without Qt, so the shared code needs a target of its own.

## Acceptance

- AC1: One first-party Qt Core static target,
  `better_pull_review_process_support` (`BetterPullReview::ProcessSupport`),
  holds the serialized worker lane with its quarantine and the process-group
  wait, kill, resolution, kill-and-reap, background reap and abandonment.
  The gh and Git runners both use it and keep no copy of their own. The
  target depends on no first-party layer, and the architecture document and
  root `AGENTS.md` record it as shared adapter support.
- AC2: gh's background reap has a deadline. A child that outlives it is
  abandoned with a typed `reap_failure`, the lane stays quarantined so later
  requests fail with `reap_failure` without launching, and destroying the
  runner returns. A focused test fails on the old unbounded reap and passes
  on the new one.
- AC3: The architecture boundary gate owns the new target. It refuses a
  QProcess or kill surface outside the designated owners, a support target
  that links another layer, and a lane that loses its quarantine, watchdog or
  bounded reap. The platform header stays the only Windows descriptor and
  handle-list builder.
- AC4: Windows-only code paths are unchanged in behaviour and as close as
  possible in text. The Windows follow-ups below need the Windows lane.

## Windows follow-ups (need the Windows lane, not done here)

- gh on Windows gets the standard-handle list but no Job Object, so a gh
  descendant survives cancel and timeout. Git puts its child in a
  kill-on-close job; gh should take the same containment.
- `src/adapters/agent/src/windows_agent_process.cpp` keeps its own native
  Windows supervision. Whether it can share any of this lane needs a build
  and a test on Windows.

## Evidence

- AC1: [process_supervision.hpp](../../../src/adapters/process_support/include/bpr/adapters/process_support/process_supervision.hpp)
  and [process_supervision.cpp](../../../src/adapters/process_support/src/process_supervision.cpp)
  hold `serialized_process_lane` and the group wait, kill, resolution,
  kill-and-reap, background reap and abandonment. Each runner keeps a small
  `process_lane_traits` (its execute, typed failure mapping and bounded
  background reap) and a `process_worker` that validates, builds its item and
  submits it. The gh runner went from 1,330 to 1,152 lines and the Git runner
  from 7,575 to 7,384, with 277 and 265 lines removed. The shared target is
  426 lines, most of them documentation. The architecture document's target
  list and dependency rules and the root `AGENTS.md` table record the shared
  adapter support target.
- AC2: gh's background reap is now `process_support::finish_background_reap`
  under `github_cli_process_max_reap_timeout`. Git's 2-second reap is
  unchanged. `github_cli_process_runner` adds
  `test_unreapable_child_is_abandoned`, which uses a new seam call,
  `child_survives_kill`, to stand in for a child that outlives SIGKILL. The
  first request fails with `reap_failure`. After the background deadline a
  later request fails at once with `reap_failure` and never launches.
  Destroying the runner returns within 2 seconds. With the old unbounded loop
  temporarily restored, the suite failed with "destroying the runner returns
  although its child never exited". With the bounded reap it passes.
- AC3: `VerifyArchitectureBoundaries.cmake` makes the support source and
  header designated QProcess owners. It requires a static target that links
  Qt Core only and includes no first-party layer, adapter or platform header.
  It refuses launch, environment, Windows job, descriptor and handle-list
  surfaces there, and pins the group kill, the bounded reaps, quarantine
  before publication, permanent quarantine after abandonment, the watchdog,
  exception recovery and one joined worker thread. Both runners must submit
  through the lane. `architecture_boundary_tests.py` adds
  `test_shared_process_supervision_contract` and retargets the gh lifecycle
  mutations; all 152 tests pass. The descriptor-builder and handle-list owner
  lists are unchanged.
- AC4: The Windows blocks in both runners are textually unchanged: the
  launch restriction, the `restricted()` check, the Git job's `create`,
  `holds` and `place_in_job`. Git's kill-and-reap still terminates its job on
  every kill attempt, through the shared reap's tree-kill parameter. Nothing
  was compiled for Windows here.
- Gates: `core_boundary_contract`, `architecture_boundaries`,
  `architecture_boundary_contract`, `github_cli_process_runner`,
  `gh_cli_transport`, `gh_cli_git_smart_transport`, `github_capability_probe`,
  `git_process_contract`, `git_cache_contract`, `git_sparse_cache_contract`,
  `git_pristine_remount_recovery` and `fsck_severity_fixture_contract` pass on
  macOS. clang-format 22 was applied, and the documentation gate passes.
