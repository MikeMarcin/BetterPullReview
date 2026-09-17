# dsp-003 Linux confirmation runs

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-003` (station
  dispatch `bpr-linux/int-5`), mandate r2, task
  `task-20260917-lnx-dsp3-confirm`. Station `bpr-linux` (`stn-3f7fcc92`,
  `wsl2`), branch `linux-lane`, authority `lane-commit`. Base
  `84ac5e0c75c37132454224adb13c2b8e6d974655`. RFC-BPR-0029 base
  `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d`.
- **Scope.** Confirmation only. BPR-0829, BPR-0813, BPR-0831, BPR-0820,
  BPR-0835 and BPR-0825 are the integrator's cards. This file is evidence
  only: no card is edited or moved, and failures are recorded, not fixed.
- **State.** Every step is recorded below. Steps 0 to 5 and 7 pass.
  Step 6 (BPR-0835) fails on Linux as on macOS, and a rerun fails the same
  way. A read-only debugger pass named the false terms (see "Step 6").
- **Commands.** None is active. No child agent was used.
- **Next.** BPR-0835 belongs to the Mac owner. This file adds the Linux
  reproduction and the terms that are false.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-dsp3-confirm/`.
  None is committed.

## Station, revision and build

Station `bpr-linux` ran these on JaegAW (WSL2) on 2026-09-17, on branch
`linux-lane` at `84ac5e0c75c37132454224adb13c2b8e6d974655`, a clean tree.

| Fact | Value |
|---|---|
| Kernel | `6.18.33.2-microsoft-standard-WSL2`, x86_64 |
| Distribution | Ubuntu 26.04 LTS |
| CPU and memory | 20 logical CPUs, 62 GiB |
| Compiler | GCC 15.2.0 (`Ubuntu 15.2.0-16ubuntu1`), `/usr/bin/c++` |
| Build tools | CMake 4.4.2, Ninja 1.13.2 |
| Qt | 6.10.3 `gcc_64` from the project's dependency cache |
| Git and test Python | Git 2.53.0; `~/.local/bin/python3` 3.13.15 |
| Environment | No `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, `CMAKE_*`, `QT_*` or `CTEST_*` variable set |
| Secret Service | The `login` collection read `(<false>,)` before every course group |

### Step 0: stock configure and full build

- **Configure.** `cmake --preset dev --fresh`: rc 0, 8.8 s, 12:29:14Z.
  `CMAKE_CXX_FLAGS` is empty.
- **Build.** `cmake --build --preset dev --clean-first`: rc 0, 1249 of 1249
  steps, 141.8 s, ending 12:31:45Z. The log has **0** `warning:` lines and
  0 `error:` lines. `ninja -n` then reported no work.
- **`-Werror` is in effect.** `cmake/Warnings.cmake` adds `-Werror` for every
  target that calls `bpr_enable_warnings`. In `compile_commands.json`, 412 of
  414 compile units carry it. The two without it are the vendored
  `sqlite3.c` and `src/adapters/logging/src/quill_log.cpp`.

### Step 1: BPR-0831, the large review benchmark compiles

The full build compiled target `bench_better_pull_review_large_review`: step
1221 of 1249 is `Building CXX object
tests/end_to_end/CMakeFiles/bench_better_pull_review_large_review.dir/large_review_benchmark.cpp.o`.
Step 1247 links its output, `tests/end_to_end/better_pull_review_large_review`.
That compile unit carries `-Wall` and `-Werror`, and it produced no
diagnostic. The source at the base is `e1424320` ("leave no statement after
the benchmark's non-Apple refusal"). The benchmark was built, not run: on
Linux it throws its Mac-only refusal by design.

### Names and how the rows ran

`ctest --preset dev -N` lists 362 tests. Each of the 44 names in steps 2 to
7 matched exactly one row with `ctest --preset dev -N -R '^<name>$'`
(`names-check.txt`). No row was substituted.

Each group ran serially with `ctest --preset dev -j1 -V -R '^(...)$'`, one log
per group (`<group>.ctest.log`). Timings and exit codes are in
`groups.summary`, and each test's ctest result line is in `groups.results`.
The Secret Service check ran immediately before each group.

## Results

| Step | Card | Result on Linux | First failing marker | Linux-specific? |
|---|---|---|---|---|
| 0 | build | rc 0, 0 warnings, `-Werror` in effect | none | n/a |
| 1 | BPR-0831 | Benchmark target compiled | none | n/a |
| 2 | BPR-0829 | Pass, 5 of 5, each `SMART_ACQUISITIONS:4` | none | n/a |
| 3 | BPR-0813 | Pass, 11 of 11 | none | n/a |
| 4 | BPR-0820 | Pass, 16 of 16 | none | n/a |
| 5 | portable test changes | Pass, 8 of 8 | none | n/a |
| 6 | BPR-0835 | **Fail, 2 of 2, twice** | `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0`; the full-agent row also prints `agent-finding-full-agent:0:0:802982:0:0:0` | No |
| 7 | BPR-0825 | Pass, 6 of 6 | none | n/a |

## Step 2: BPR-0829, the anonymous public preview course

Five separate `ctest` runs of `application_anonymous_public_preview_public_path`,
12:32:41Z to 12:32:58Z. The registration expects
`PUBLIC_PREVIEW_SMART_ACQUISITIONS:4` with the epoch phase budget
(`--automated-run-budget-ms 120000`, ctest timeout 170 s).

| Run | Result | Duration | `OCCURRENCES` | `SMART_ACQUISITIONS` | `PUBLIC_PREVIEW_WAITING_FOR` |
|---|---|---|---|---|---|
| 1 | Pass | 1.57 s | 4 | 4 | not printed |
| 2 | Pass | 1.52 s | 4 | 4 | not printed |
| 3 | Pass | 1.57 s | 4 | 4 | not printed |
| 4 | Pass | 1.54 s | 4 | 4 | not printed |
| 5 | Pass | 1.54 s | 4 | 4 | not printed |

- Every run printed the whole block in order, from `PUBLIC_PREVIEW_COMPOSED:true`
  through `PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:Incomplete history · ambiguous
  occurrences · unresolved local records · display order is not push order`,
  `PUBLIC_PREVIEW_SMART_ACQUISITIONS:4` and `PUBLIC_PREVIEW_ENABLED_OK`.
- **Duration, compared with macOS.** A Linux run takes about 1.5 s. The card
  records 32.7 to 35.6 s on macOS, and about 7.5 s for each import there. The
  count of 4 means all three retention imports finished before the writes
  were offered, which is what the wait is for. Why the imports are faster on
  this host was not investigated. The 1-minute load average was 6 to 7.6
  (the build had just finished).
- **Before the fix,** this station saw 2, 2, 1, 1 and 2 at dsp-002's base
  ([dsp-002 Linux confirmation](dsp2-linux-confirmation.md#bpr-0829)). The
  change settles the count on Linux too.
- **Offscreen by registration:** `QT_QPA_PLATFORM=offscreen`,
  `QT_STYLE_OVERRIDE=Windows`.

## Step 3: BPR-0813, the Linux store courses

Group `0813-stores`, 12:33:14Z, 48.80 s, ctest rc 0.

| Test | Result | Duration | Marker |
|---|---|---|---|
| `sqlite_device_binding_storage` | Pass | 0.43 s | `device-binding-storage-tests:pass` |
| `sqlite_profile_registry_storage` | Pass | 0.34 s | `profile-registry-storage-tests:pass` |
| `sqlite_window_review_restoration_storage` | Pass | 0.16 s | `window-review-restoration-storage-tests:pass` |
| `sqlite_profile_activation` | Pass | 4.15 s | `profile-activation-tests:pass` |
| `sqlite_profile_runtime_lease` | Pass | 0.00 s | `profile-runtime-lease-tests:pass` |
| `sqlite_profile_storage_root` | Pass | 0.00 s | `profile-storage-root-tests:pass` |
| `sqlite_epoch_catalog_storage_contract` | Pass | 0.22 s | `sqlite-epoch-catalog-storage-tests:pass` |
| `application_device_binding_restart` | Pass | 28.91 s | `device-binding-restart-tests:pass` |
| `application_profile_startup_public_path` | Pass | 1.71 s | `profile-startup-public-path:pass` |
| `application_profile_management_public_path` | Pass | 2.86 s | `profile-management-public-path:pass` |
| `application_profile_multi_window_public_path` | Pass | 9.99 s | `profile-multi-window-public-path:pass` |

- Five of the SQLite rows have no pass expression and pass on exit status.
  Each still printed its `:pass` line.
- **What this covers.** BPR-0813 changed `catalog_database.cpp`,
  `storage_internal.hpp` and the three store sources, which are shared with
  POSIX. Those stores still open and reopen on Linux. The new
  `sqlite_private_store_open` contract is registered only `if(WIN32)`
  (`tests/sqlite_contract/CMakeLists.txt`), so it does not exist here.
- **Offscreen by registration:** the four `application_*` rows run
  `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`. The seven
  `sqlite_*` rows set no Qt platform.

## Step 4: BPR-0820, the private-profile rows

Group `0820`, 12:35:07Z, 411.89 s, ctest rc 0.

| Test | Result | Duration | Markers |
|---|---|---|---|
| `application_public_path` | Pass | 0.32 s | `PROFILE_PRIVATE_OK`, `PUBLIC_PATH_OK`, `CLI_UNAVAILABLE_OK` |
| `application_public_path_ghes_cli_unavailable` | Pass | 0.33 s | `PROFILE_PRIVATE_OK`, `PUBLIC_PATH_OK`, `CLI_UNAVAILABLE_OK` |
| `application_device_authorization_public_path` | Pass | 0.37 s | `PROFILE_PRIVATE_OK`, `DEVICE_FLOW_REGISTERED_SELECTION_OK` |
| `application_device_authorization_unavailable` | Pass | 0.39 s | `PROFILE_PRIVATE_OK`, `DEVICE_FLOW_REGISTERED_SELECTION_OK` |
| `application_composition_capabilities_inside_boundary` | Pass | 0.30 s | `PROFILE_PRIVATE_OK`, `COMPOSITION_TEST_BOUNDARY:inside`, `COMPOSITION_BROWSER:recording` |
| `application_composition_capabilities_product_run` | Pass | 0.31 s | `PROFILE_PRIVATE_OK`, `COMPOSITION_TEST_BOUNDARY:outside`, `COMPOSITION_BROWSER:system` |
| `application_device_authorization_ghes_refusal` | Pass | 0.33 s | `PROFILE_PRIVATE_OK`, `DEVICE_FLOW_GHES_BEGIN_REFUSED:true`, `DEVICE_FLOW_GHES_REFUSAL_OK` |
| `application_device_authorization_release_contract` | Pass | 0.37 s | `PROFILE_PRIVATE_OK`, `DEVICE_FLOW_REGISTERED_SELECTION_OK` |
| `application_anonymous_public_preview_unavailable` | Pass | 0.34 s | `PROFILE_PRIVATE_OK`, `PUBLIC_PREVIEW_UNAVAILABLE_OK` |
| `application_anonymous_public_preview_release_contract` | Pass | 0.32 s | `PROFILE_PRIVATE_OK`, `PUBLIC_PREVIEW_UNAVAILABLE_OK` |
| `application_anonymous_public_preview_enabled_probe` | Pass | 0.33 s | `PROFILE_PRIVATE_OK`, `PUBLIC_PREVIEW_ENABLED_OK` |
| `application_anonymous_public_preview_live_candidate_wiring` | Pass | 0.32 s | `PROFILE_PRIVATE_OK`, `PUBLIC_PREVIEW_LIVE_CANDIDATE_OK`, `PUBLIC_PREVIEW_REQUESTS:0` |
| `application_render` | Pass | 0.37 s | `PROFILE_PRIVATE_OK`, `RENDER_OK` |
| `runtime_process_surface` | Pass | 0.54 s | `RUNTIME_SURFACE_OK` |
| `runtime_audit_contract` | Pass | 0.09 s | `Ran 18 tests`, `OK (skipped=1)` |
| `application_composition_contract` | Pass | **406.84 s** | `Ran 7 tests in 406.786s`, `OK` |

- **The private profile is in effect.** All 13 `--self-test` and
  `--render-evidence` rows printed `PROFILE_PRIVATE_OK`, and no command line
  names a data root. The runtime surface row's registered command is
  `verify_runtime_surface.py --private-data-root <application>`.
- **The audit contract's output.** Its skip is
  `test_darwin_native_query_detects_a_real_direct_child`, which needs macOS
  libproc. Its stderr line `private data root was not composed:
  /tmp/bpr-runtime-surface-…/data` is printed by the negative case
  `test_private_root_that_was_never_composed_fails_and_is_removed`.
- **Timeout margin.** `application_composition_contract` performs nested
  configures and a tests-disabled Release build. It used 406.84 s of its
  600 s timeout, at a 1-minute load average of about 2 when the group began.
  No earlier Linux duration for this row is recorded. Other records put it at
  107 to 293 s on other hosts.
- **Offscreen by registration.** The 13 application rows run
  `QT_QPA_PLATFORM=offscreen` (`application_render` also sets
  `QT_SCALE_FACTOR=1.25`). `verify_runtime_surface.py` sets
  `QT_QPA_PLATFORM=offscreen` for the process it launches, and so does
  `composition_contract_tests.py` for its runtime launches. The audit contract
  starts no Qt process.

## Step 5: portable test changes

Group `portable`, 12:42:33Z, 163.75 s, ctest rc 0.

| Test | Result | Duration | Marker |
|---|---|---|---|
| `application_poller_liveness` | Pass | 0.21 s | `github-application-boundary-tests:pass`, `github-application-liveness-tests:pass` |
| `application_epoch_ordinary_update_recreated` | Pass | 4.31 s | `EPOCH_ORDINARY_RECREATED_OK:1` |
| `application_epoch_portable_occurrence_two_catalogues` | Pass | 12.05 s | `EPOCH_PORTABLE_TWO_CATALOGUES_OK:3` |
| `architecture_boundary_contract` | Pass | 134.71 s | `Ran 132 tests`, `OK` |
| `architecture_boundaries` | Pass | 8.98 s | `Application, provider, and Qt architecture boundaries verified` |
| `test_network_isolation_manifest` | Pass | 1.18 s | `TEST_NETWORK_ISOLATION_OK:configured=362:registrations=344:backend=linux-network-namespace` |
| `test_network_isolation_structure_contract` | Pass | 0.75 s | `Ran 58 tests`, `OK (skipped=1)` |
| `application_review_round_trip_composition_contract` | Pass | 1.55 s | `REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK` |

- **The round-trip pins hold at this base.** The contract passes with the
  pins resealed in `df235fa1`, which covers the BPR-0829 `main.cpp` change
  (`7555cba4`).
- **The structure contract's output.** The skip is the Mach-O composition
  symbol probe, which runs only on `darwin`. Its
  `TEST_NETWORK_ISOLATION_RUNNER_FAILED:profile-template-contains-network-allow`
  line is printed by a negative case of `run_network_isolated_test.py`.
- **Designated-live rows: none exists on Linux.** The changed
  `designated_live_macos_capsule_tests.py` is registered as
  `designated_live_macos_capsule_contract` only `if(APPLE)`
  (`tests/CMakeLists.txt`). `ctest -N -R 'designated|capsule|macos'` lists
  `designated_live_mutation_gate`,
  `designated_live_mutation_runner_digest_mismatch` and
  `application_designated_review_authority_bootstrap`. They are not in this
  step's list, and they were not run. `tests/network_isolation/`,
  `tests/application/` and `src/application/` are unchanged since dsp-002's
  base.
- **Offscreen by registration.** The poller, both epoch rows and the round
  trip run `QT_QPA_PLATFORM=offscreen` with `QT_STYLE_OVERRIDE=Windows`, and
  both epoch scripts also set `QT_QPA_PLATFORM=offscreen` for the launches
  they make. The architecture and network-isolation rows start no Qt process.

## Step 6: BPR-0835, the agent finding composition contracts

Group `0835`, 12:45:40Z, 2.62 s, ctest rc 8. Diagnostic rerun `0835-rerun`,
12:46:25Z, 2.61 s, ctest rc 8.

| Test | Run | Rerun | Printed |
|---|---|---|---|
| `application_agent_finding_composition_contract` | **Fail**, 1.29 s | **Fail** | `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0`, `panel-status:Finished: success` |
| `application_agent_finding_full_agent_contract` | **Fail**, 1.32 s | **Fail** | the same two lines, then `agent-finding-full-agent:0:0:802982:0:0:0` |

- **First failing marker.** ctest reports `Required regular expression not
  found` for `agent-finding-composition:pass` and `agent-finding-full-agent:pass`.
  The first line the course prints is
  `agent-finding-positive-evidence:1:1:1:1:1:1:0:1:0:0`. The rerun printed
  identical lines.
- **Not Linux-specific.** The card records the same two lines on macOS at
  `5a2e264a` and at `2ee49a16`. The path construction below has no platform
  branch.
- **Every printed field has its expected value.** The ten fields are
  deliveries 1, queued findings 1, frozen files 1, snapshot writes 1, workshop
  proposals 1, draft commands 1, `helperRefusal` 0 (`mcp_helper_refusal::none`,
  the passing value), MCP completions 1, submissions 0 and target
  observations 0. The failing terms are ones the line does not print.

### Which terms are false (read-only diagnosis)

`lldb-22` ran the `positive` course four times inside the network sandbox
with the ctest environment. It sampled the verdict at
`agent_finding_composition_public_path.cpp:791` (`if (!exact)`) and changed
no source and no value. The scripts and logs are in `0835-lldb/`.

| Term | Value |
|---|---|
| `exact` | false |
| `noGithubMutation` | true (0, 0, 0) |
| `positiveMarker` | false |
| Its composed flags, queued findings 1, workshop proposals 1, draft body, side-chat presentations 9 | all as required |
| `exact_helper_launch(evidence, snapshotRoot)` | **false**: `launch.WorkingDirectory == aSnapshotRoot.string()` fails |
| `exists(snapshotRoot / "binding")`, `exists(snapshotRoot / "alignment")` | **false** |
| Every other conjunct of `exact` (statistics, claim text, anchor state, workshop texts, draft bodies, MCP starts 1 and refusals 0) | as required |

- **What was observed.** The helper's working directory is
  `<tmp>/data/mcp-snapshot/window-1-generation-1`. The course expects
  `<tmp>/data/mcp-snapshot`. With the process stopped at the verdict,
  `mcp-snapshot/` holds only `window-1-generation-1/`, which contains
  `binding`, `alignment` and `files/`.
- **Where the subdirectory comes from.** `src/app/src/github_application.cpp`
  (~6410) sets `McpSnapshotRoot = ApplicationDataRoot / "mcp-snapshot" /
  ("window-" + … + "-generation-" + …)`. Source history (`git log -S`) shows
  that `826fb871` ("Finish BPR-0705", 2026-09-08) made this change. At its
  parent the root was `ApplicationDataRoot / "mcp-snapshot"`. The contract
  (`bacd5177`, BPR-0504) still expects the old root.
- **The full-agent row fails a second term, `noSecondEdge`.** Counting the
  contract's tokens in the 802982-byte composition source, every count matches
  except `->submit(`, which occurs **3** times against the required 1:
  `SetupDestination->submit(` at ~5723 and ~6067, and
  `ReviewLedgerHost->submit(` at ~7406. Source history puts the count at 1
  from `5fc45d2f`, 2 from `554564f8` ("Finish BPR-0689", 2026-09-08) and 3
  from `91383cc4` ("Finish BPR-0733", 2026-09-11). `554564f8` is an ancestor
  of `826fb871`.
- **Limits.** The revisions are inferred from source history, not from runs
  at those revisions. An earlier failure on some other term is not excluded.
  Whether the course or the product is wrong is the owner's call. lldb
  reported a DWARF base-class resolution error for `std::optional`, and its
  formatter showed `McpLaunch` as empty. Reading the object's bytes showed it
  engaged, and the working directory above was read from that memory.
- **Offscreen by registration:** `QT_QPA_PLATFORM=offscreen`,
  `QT_STYLE_OVERRIDE=Windows`.

## Step 7: BPR-0825, POSIX sanity

Group `0825`, 12:46:32Z, 126.18 s, ctest rc 0.

| Test | Result | Duration | Marker |
|---|---|---|---|
| `agent_workspace_ownership` | Pass | 0.73 s | `AGENT_WORKSPACE_OWNERSHIP_OK` |
| `git_process_contract` | Pass | 5.36 s | `Git process contract passed` |
| `agent_parent_death` | Pass | 23.93 s | `agent-parent-death:owner-pid:pass`, `agent-parent-death:owner-group:pass` |
| `qt_agent_launch_canary` | Pass | 70.40 s | `agent-launch-canary-tests:pass` |
| `application_agent_crash_recovery` | Pass | 18.69 s | `agent-composition-kill:owned-workspace-reclaimed entries=18`, `agent-composition-kill:pass` |
| `application_agent_materialization_crash_recovery` | Pass | 7.07 s | `agent-materialization-kill:pass` |

- **What this covers.** BPR-0825 changed
  `src/adapters/git/src/installed_git_process.cpp`,
  `windows_agent_supervisor.cpp` and `agent_workspace_ownership_tests.py`.
  The POSIX Git contract, the ownership and parent-death courses, the launch
  canary and both crash-recovery courses still pass on Linux.
- **Offscreen by registration.** `qt_agent_launch_canary` and both
  crash-recovery rows run `QT_QPA_PLATFORM=offscreen` with
  `QT_STYLE_OVERRIDE=Windows`. `agent_workspace_ownership`,
  `git_process_contract` and `agent_parent_death` set no Qt platform.

## New defects

None is carded, as the envelope allows no reservation. Only one observation
is not covered by an existing card: `application_composition_contract` used
406.84 s of its 600 s timeout on this host (step 4). BPR-0835's false terms
are that card's own scope.

Logs: `step0.summary`, `dev.configure.log`, `dev.build.log`, `ctest-N.txt`,
`names-check.txt`, `ctest-show-only.json`, `groups.summary`,
`groups.results`, `<group>.ctest.log`, `docgate-*.log` and `0835-lldb/`.
