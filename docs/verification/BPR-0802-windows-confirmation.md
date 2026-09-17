# BPR-0802 Windows confirmation

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-hardening`, step 1. The base is
`de58ebc66232a9dc03456bb34a7c25ccc7b84453`, which contains [[BPR-0802]]'s macOS
half (`49d7df69`). The card is integrator-owned; this file is evidence only.

**Host.** Windows 11 Home 10.0.26200; Visual Studio 18 Community, MSVC
14.51.36231; Qt 6.10.3 `msvc2022_64`. The shell token is not elevated.

**Build.** `cmake --preset windows-dev` and a full `cmake --build` of
`build/windows-dev` (Debug). One target fails to compile:
`bench_better_pull_review_large_review`. In `large_review_benchmark.cpp`, lines
180–200 follow an unconditional non-Apple `throw`, so MSVC reports C4702
(unreachable code) as an error under `/WX`. That file is unchanged by the merge.
It is reported as a new defect, not fixed here. Every other target built.

## The manifest gate

`ctest --preset windows-dev -R "^test_network_isolation_manifest$" -V`

It reports **24 findings**, against 25 before the merge. The comparison against
the pre-merge finding list (slice (a) of [[BPR-0622]]) is:

- **Removed:** `raw add_test ownership is ['cmake/BprTestIsolation.cmake',
  'tests/github_contract/CMakeLists.txt'], expected only
  cmake/BprTestIsolation.cmake`, as expected.
- **Wording changed, same finding:** `sqlite_profile_activation does not start
  with [...network_sandbox.exe]` previously ended `: None`. It now names the
  row's command, `['.../tests/sqlite_contract/Debug/profile_activation.exe']`.
  The finding, its row and its cause are unchanged; only the reported command
  is now resolved.
- **No other change.** The remaining 23 findings are byte-identical. They are
  the eight unpinned uncontained rows, three findings each, that [[BPR-0812]]
  owns, minus the one whose wording changed.

## The structure contract

`test_network_isolation_structure_contract` passed (3.22 s):
`Ran 58 tests`, `OK (skipped=10)`.

## Seam environment before and after

`ctest --preset windows-dev --show-only=json-v1` was captured twice:

- **Before:** from the pre-merge build tree at `c94e02b1`, before
  reconfiguring.
- **After:** at `de58ebc6`, after reconfiguring.

| Row | `ENVIRONMENT_MODIFICATION` entries before / after | Equal |
|---|---|---|
| `application_public_path` | 23 / 23 | yes |
| `agent_parent_death` | 23 / 23 | yes |

Across all 354 configured rows no `ENVIRONMENT_MODIFICATION` value changed, and
no row was added or removed. `gh_cli_git_smart_transport` is not registered in
the Windows configuration, before or after, so the `MACOS_UNSANDBOXED_TESTS`
exception has no Windows row to apply to.

## The three courses

| Row | Result |
|---|---|
| `application_device_authorization_browser_handoff` | Passed, 6.73 s |
| `application_composition_capabilities_inside_boundary` | **Fails contained**: the probe prints only `STARTUP_FAILED`. Run by hand uncontained with the same arguments and the marker set, it prints `COMPOSITION_TEST_BOUNDARY:inside`, `COMPOSITION_BROWSER:recording` and `COMPOSITION_CLI_READ:unavailable`, and exits 0. The row is registered contained, so the gate does not report it. No person's application was running (`tasklist`), so this is not [[BPR-0820]]'s `PROFILE_ALREADY_OPEN`. Reported as a new defect; the contained cause was not isolated further. |
| `application_poller_liveness` | **Fails before the boundary subtest**: `FAILED: application startup succeeds`. The boundary line `github-application-boundary-tests:pass` is not printed, because the first in-process start fails. A temporary diagnostic (reverted) showed `github_application_startup_failure::registry`. The test's data root is a bare `QTemporaryDir`, which on Windows lacks the owner-private descriptor the profile registry requires. The same failure appears with `TEMP` pointed at an owner-private directory. So on Windows neither the boundary subtest nor [[BPR-0827]]'s tick subtest is reached. Reported as a new defect. |
