# BPR-0899 durable layout write checkpoint

Assignment r1, 2026-09-25; run `bpr0899-20260925`, segment `r1`, milestone
`durable-layout-drain`, coordinator `/root`, leaf `/root/bpr0899_layout_write`.
The mandate authorizes unattended standalone Codex implementation and ordered
commit handoff. Source/tracker root is the isolated
`/Users/jaeger/.codex/worktrees/bpr0899-layout-write/BetterPullReview`, branch
`codex/bpr0899-layout-write`; baseline is
`9a0b44d56ab645de10afffd7e69a4ca0a3fe8891`. Runtime evidence is under
`build/bpr0899` in that worktree. No RFC-0052 review candidate, primary checkout,
other issue, remote repository or live provider was changed.

## Candidate and decision

The application source is bound by the LF-normalized composition digests:

- `github_application.cpp`: `5bc3de4d81fc806d9ad3a1b9dd0470200e5b00030daaddd0edc2345aea6cbbb6`.
- Production app manifest, including the new private header:
  `29225f6798667170da6f68217ef5e1044c70ebc5dda60db2aa31cad6c9ec0c7e`.
- Production mutation branch remains
  `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.

The [card](../issues/60-done/BPR-0899-the-layout-write-failed-flag-cannot-tell-a-refusal-from-a-write-in-flight.md)
records the design and self-review. The selected policy retains the current
host drain: one active store plus one coalesced latest candidate, no new ingress
or automatic retry after Quit. SQLite's existing two-second busy wait stays in
force. This is a bound on work and lock waits, not an OS I/O deadline. The
port's last terminal refusal and explicit in-flight count have distinct
meanings. A later successful store clears the old refusal. An exception is
normalized to `unavailable` inside the port, preserving the host's existing
mapping and making its cleanup diagnostic accurate. `in-flight-at-drain` is
retired because cleanup observes only after workbench-host quiescence.

## Changed paths

- `src/app/src/github_application.cpp`: compose the extracted port and report
  only a completed refusal after the existing host drain.
- `src/app/src/profile_window_layout_port.hpp`: same concrete service, port,
  explicit in-flight observation and terminal result bookkeeping.
- `tests/end_to_end/profile_window_layout_drain_tests.cpp` and its
  `CMakeLists.txt` registration: real port/host/SQLite synchronization,
  refusal/recovery and latest-layout shutdown.
- `tests/end_to_end/profile_window_layout_public_path_tests.cpp`: require a
  successful desktop exit after layout draining, alongside existing exact
  durable rows, independent windows, geometry, restart and Close checks.
- `tests/end_to_end/application_review_round_trip_composition_contract.cmake`:
  include the extracted header in both inventories and reseal reviewed scope.
- `docs/architecture/system-architecture.md`, BPR-0899, this checkpoint and
  `docs/rfcs/plans/bpr0899-layout-write/{milestone-plan.json,source.json}`:
  adopted obligations, lifecycle meaning, review, acceptance and handoff.

## Verification

Executed on macOS arm64 with Apple Clang 21.0.0.21000101 and Qt 6.11.2, using a fresh
worktree build and the existing read-only CPM source cache. No target-native
Linux or Windows observation is claimed.

- Before editing production, `application_profile_window_layout_public_path`
  passed at the baseline (24.30 s).
- The first new contract failed because its test root was missing; it now
  creates its own private `data` child through the existing storage helper.
- The new contract passes with real SQLite. A negative control changes the
  predicate to report an in-flight write as failed; that fails specifically
  at the Quit/in-flight assertion (0.33 s). The mutation was removed.
- Final focused integration passes 7/7: `architecture_boundaries`,
  `architecture_boundary_contract`, `application_workbench_host`,
  `sqlite_workbench_settings_storage`,
  `application_review_round_trip_composition_contract`,
  `application_profile_window_layout_drain` and
  `application_profile_window_layout_public_path`.
- `python3 scripts/verify_documentation.py --root .` passes;
  `python3 tests/scripts/verify_documentation_tests.py` passes all 64 tests;
  scoped board validation and `git diff --check` pass.
- `python3 _coding-standard/scripts/format_check.py` on the four changed C++
  files passes with clang-format 23.1.1. No source behavior changed after the
  integrated run; the negative predicate is absent from the final source.

The build command was `cmake --build --preset dev --target
 test_profile_window_layout_drain test_profile_window_layout_public_path
 test_better_pull_review_application_workbench_host_tests
 test_better_pull_review_workbench_settings_tests app_better_pull_review -j 4`.
The test command was `ctest --preset dev -R
 '^(application_workbench_host|application_profile_window_layout_drain|application_profile_window_layout_public_path|sqlite_workbench_settings_storage|application_review_round_trip_composition_contract|architecture_boundaries|architecture_boundary_contract)$'
 --output-on-failure`. Full transient output remains in
`build/bpr0899/{integration,final-build,negative,documentation,documentation-tests,format-final}.log`.
The direct contract does not inject allocator/mutex exceptions; the exception
mapping was reviewed. These portable changes were exercised on this Mac only.

## Ownership and continuation

The implementation is a non-delegating leaf. The only new worktree writer is
this assignment; other repository writers remain confined to their own scopes.
No attended criterion exists on BPR-0899, so no review session/server is needed.
Its source-only adoption, selection, mandate, binding and observations are
retained with this checkpoint and in `build/bpr0899`.

The fresh worktree is not separately registered with MASK. The installed
MASK-managed skill payload was used directly; the repository documentation
import uses a local ignored `.agents/skills/implement-rfc` link to that same
payload. No global skill deployment was changed and configured model routing
is not claimed as independently observed serving.

At handoff, the coordinator may cherry-pick the ordered commits and rerun the
layout/composition selection if concurrent changes overlap those files. No
source or tracker work outside this standalone issue is authorized by this
handoff. Native release qualification and all unrelated human decisions retain
their existing owners.

## Final engineering handoff

Product candidate: `e7774b61107b4500dfbaaa914b038e186ccc2e87`. The seven-row run (139.06 s)
compiled and exercised these exact source bytes before that commit; subsequent
changes are acceptance/checkpoint bookkeeping only. No test/build process,
child writer, fixture app, review server or remote job remains from this task.
The failed negative-control executable was rebuilt from the restored candidate
before the passing integrated run. Test roots were cleaned by their owners.
The ignored local skill link and build evidence are retained for continuation.

Ordered implementation history starts with `f7133ff1` (bounded plan and Ready),
`c7e98046` (claim), then `e7774b61` (implementation, review, tests and semantic
documentation). The final acceptance/checkpoint commit follows them. The
coordinator owns cherry-picking; no push or integration into main has run here.
Writer ownership is released at the final handoff. There are no outstanding
BPR-0899 acceptance decisions, attended sessions, or independent review gates.
Linux/Windows execution and injected allocation/mutex exception testing were
not performed; no such observation is implied by completing this portable
standalone card.
