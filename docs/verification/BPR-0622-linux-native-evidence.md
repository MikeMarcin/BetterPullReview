# BPR-0622 Linux native verification evidence

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  `bpr-linux/int-1`), mandate r2 with integrator amendments 1 and 2, task
  `task-20260917-lnx-evidence`. Station `bpr-linux` (`stn-3f7fcc92`, `wsl2`),
  branch `linux-lane`, authority `lane-commit`. Cards in order: BPR-0622
  (evidence only; its card stays In Progress and unedited), BPR-0799, BPR-0731.
- **BPR-0622 state.** The Linux run of the Unix recovery set is recorded below.
  Five of eight pass. The three failures match known platform-independent
  defects; none of them is Linux-specific. The card stays In Progress: the
  non-Unix implementation and Windows native evidence remain open.
- **Next step.** BPR-0799, then BPR-0731, from the same `app-release` and `dev`
  trees. Their evidence lives in their own files.

## Station, task and revisions

Station `bpr-linux` ran task `task-20260917-lnx-evidence` on JaegAW (WSL2), on
branch `linux-lane`.

- **Dispatched base:** `17f495fea40f6985c0d2f6b0430c7a71062f8085`. It includes
  BPR-0666's `agent_parent_death` guard-release race fix.
- **Evidence revision:** `9bbe3604a19416033e0543523a54d3fabab27b4a`. It is that
  base plus one Linux compile fix in BPR-0622's Unix path, described below.
  Every result in this file was produced at `9bbe3604`.

## Host

| Fact | Value |
|---|---|
| Kernel | `6.18.33.2-microsoft-standard-WSL2`, x86_64 (WSL2, not a full-system VM) |
| Distribution | Ubuntu 26.04 LTS |
| CPU and memory | 20 logical CPUs, 62 GiB |
| Compiler | GCC 15.2.0 (`Ubuntu 15.2.0-16ubuntu1`), `/usr/bin/c++` |
| Build tools | CMake 4.4.2, Ninja 1.13.2 |
| Qt | 6.10.3 `gcc_64` from the project's dependency cache |
| Git | 2.53.0 |
| Test Python | `~/.local/bin/python3` 3.13.15 |
| Secret Service | The `login` collection read `(<false>,)` (unlocked) before each build and course |

## The Linux build at the dispatched base

At `17f495fe`, neither Linux preset compiles with GCC 15.2 under the project's
`-Werror` (`cmake/Warnings.cmake`). These keep-going builds (`ninja -k 0`)
enumerate every failure that the dependency order let them reach.

**Tests-disabled `app-release`** (Release, `-O3`, `_FORTIFY_SOURCE=3`) fails in
these translation units:

| Translation unit | Diagnostic | First-party lines |
|---|---|---|
| `src/adapters/agent/src/unix_agent_supervisor.cpp` | `-Werror=unused-result` | 117: `static_cast<void>(::write(...))`. GCC does not accept a void cast of a `warn_unused_result` call as a use. |
| `src/application/src/profile_startup_admission.cpp` | `-Werror=maybe-uninitialized` (33) | 73, 82, 106, 128, 133, 165; `profile_startup_admission.hpp:35`; `profile_storage_authority.hpp:13` |
| `src/application/src/syntax_classification_application_host.cpp` | `-Werror=maybe-uninitialized` (2) | 313, 316, 333 |
| `src/application/src/appearance_preferences_application_host.cpp` | `-Werror=maybe-uninitialized` (1) | 666, 672 |
| `src/adapters/sqlite/src/review_ledger_database.cpp` | `-Werror=maybe-uninitialized` (2) | 1810, 1816 |
| `src/adapters/github/src/github_mutation_contract.cpp` | `-Werror=maybe-uninitialized` (4) | 986, 993, 1016, 1023, 1429 |
| `src/adapters/github/src/github_capability_probe.cpp` | `-Werror=dangling-reference` | 1468. `json_member` returns a reference into its object argument or a static, so this is a GCC heuristic false positive. |

**`dev`** (Debug) fails in these translation units:

| Translation unit | Diagnostic |
|---|---|
| `src/adapters/github/src/github_capability_probe.cpp` | `-Werror=dangling-reference`, line 1468, as above |
| `tests/sqlite_contract/profile_registry_storage_tests.cpp` | `-Werror=dangling-pointer=`, lines 97 and 195 |
| `tests/sqlite_contract/queue_enrollment_storage_tests.cpp` | `-Werror=dangling-pointer=`, line 550 |
| `tests/github_contract/github_mutation_contract_tests.cpp` | `-Werror=dangling-pointer=`, line 432 |
| `tests/github_contract/live_review_submission_provider_tests.cpp` | `-Werror=dangling-pointer=`, line 222 |
| `tests/adapters_qt/workbench_window_tests.cpp` | `-Werror=dangling-pointer=`, line 6268 |
| `tests/adapters_qt/dif_runtime_tests.cpp` | `-Werror=unused-function`, lines 169, 174 and 183 |

**Disposition.**

- The supervisor failure is confined to BPR-0622's Unix path, so it was fixed
  in `9bbe3604`. The result of `write` is now held in a local and discarded.
  What the owner observes when that exec-failure report is lost is unchanged:
  end-of-file, then the child's exit 127. That object compiles under the
  unmodified Release flags.
- Every other failure is outside BPR-0622's path and was **not** fixed. They are
  reported to the integrator as Linux GCC build defects. No card was minted.

**Evidence build flags.** Both evidence trees were configured `--fresh` with
this setting:

```sh
-DCMAKE_CXX_FLAGS="-Wno-error=maybe-uninitialized -Wno-error=dangling-reference -Wno-error=dangling-pointer -Wno-error=unused-function"
```

- These flags only downgrade those four diagnostic classes to warnings. Code
  generation is unchanged, the diagnostics still print, and every other
  warning stays an error. `unused-result` stays an error.
- Both trees then built at `9bbe3604`: `app-release` in 91 s and `dev` in 146 s.
- The warnings they printed are exactly the translation units listed above;
  nothing else warned.

## The Unix recovery set

Registered names were confirmed with `ctest --preset dev -N`. The set ran
unchanged, serially, with this command:

```sh
ctest --preset dev -j1 --output-on-failure -R '^(agent_parent_death|agent_workspace_ownership|application_agent_materialization_crash_recovery|application_agent_crash_recovery|qt_agent_launch_canary|git_process_contract|architecture_boundaries|test_network_isolation_manifest)$'
```

- **First run.** Started 2026-09-17T06:15:20Z, 106.21 s wall time, ctest exit 8.
- **Verbose rerun.** The five passing tests, with `-V`, started
  2026-09-17T06:17:32Z and took 95 s. They pass again with their course markers
  visible.
- **Repeats.** `application_agent_crash_recovery` ran two more times.

Every test runs inside `better_pull_review_network_sandbox`.

| Test | Result | Duration (first run / rerun) | Markers observed |
|---|---|---|---|
| `qt_agent_launch_canary` | Pass | 60.84 s / 61.01 s | `agent-launch-canary-tests:pass` |
| `agent_parent_death` | Pass | 20.87 s / 20.86 s | `agent-parent-death:owner-pid:pass`, `agent-parent-death:owner-group:pass`, each after `agent-parent-death:terminated` |
| `git_process_contract` | Pass | 5.29 s / 5.31 s | `Git process contract passed` |
| `agent_workspace_ownership` | Pass | 0.69 s / 0.71 s | `AGENT_WORKSPACE_OWNERSHIP_OK` |
| `application_agent_materialization_crash_recovery` | Pass | 7.06 s / 7.12 s | `agent-materialization-kill:init:pass`, `:checkout:pass`, `:cancel:pass`, `agent-materialization-kill:pass` |
| `application_agent_crash_recovery` | **Fail**, 3 of 3 | 8.20 s, 8.18 s, 8.19 s | see below |
| `architecture_boundaries` | **Fail** | 2.01 s | see below |
| `test_network_isolation_manifest` | **Fail** | 1.23 s | see below |

The Linux durations are much shorter than the Mac durations recorded on the
card, for example `git_process_contract` at 64.65 s there. The course scripts
carry no platform skip: `agent_workspace_ownership_tests.py` still drives
every SIGKILL checkpoint in its list, and the verbose rerun shows each course's pass
markers. The difference is most plausibly process-creation and sandbox cost.
It was not measured.

### What the passing courses establish on Linux

- **Lock semantics** (`flock`). In `agent_parent_death`, the supervisor is frozen
  with SIGSTOP and the application is killed, by PID and by process group. An
  independent `LOCK_EX | LOCK_NB` on the guard must still fail while the paused
  supervisor holds the shared lock. After SIGCONT, the agent and its descendant
  must stop, exclusive access must succeed, and a fresh launch must succeed.
  Both courses pass on this kernel.
- **Stop events** (`waitid`). `application_agent_materialization_crash_recovery`
  leaves the selected Git writer stopped for several supervisor poll intervals
  before it freezes the supervisor. On Darwin, `waitid(WEXITED | WNOHANG |
  WNOWAIT)` reported that stopped child as `CLD_STOPPED`, and the old supervisor
  killed it. On Linux, all three stages pass:
  - Git and the supervisor are both still running at the crash observation.
  - Two ordinary recoveries leave the workspace, the journal and the Git/SQLite
    state byte-identical while the frozen supervisor still owns the stopped
    writer.
  - After SIGCONT, the supervisor stops and reaps Git.
  - Two further recoveries then reclaim the workspace with exact state retained.

  So the `CLD_EXITED`/`CLD_KILLED`/`CLD_DUMPED` filter does not misfire on Linux.
  The course does not print the raw `si_code` Linux returns for a stopped
  child, and no separate probe was run for it.
- **Journal and quarantine.** `agent_workspace_ownership` passes on Linux:
  - the committed-phase and quarantine-rename SIGKILL checkpoints, with double
    recovery;
  - both mkdir-before-identity windows, which stay unchanged under `recover`
    and `abandon`;
  - the live-owner exclusion;
  - normal cleanup.

### Failures, classified

- **`application_agent_crash_recovery`: same failure as macOS, not
  Linux-specific.** Each of the three runs failed after about 8 s with:

  ```text
  RuntimeError: startup-recover failed (1):

  application-startup-failed:

  ```

  The message and the empty detail are identical to BPR-0808's macOS record at
  `94f4e6dc`. It was not fixed here. Two observations may help BPR-0808:
  - **The failing step.** It is the `live_recovery` step inside
    `pause_supervisor`, before any kill. That step starts a second ordinary
    `startup-recover` application on the same data root while the
    `crash-running` application is still alive. The same `startup-recover`
    invocation passes in `application_agent_materialization_crash_recovery`,
    which calls it only after the application is dead.
  - **The empty detail.** In
    `tests/end_to_end/agent_invocation_composition_public_path.cpp`,
    `wait_terms::render()` is empty only when startup reached its terminal
    state. So startup completed with an error result, and the course discards
    that result's typed reason.
  - **Hypothesis, not verified.** The live application's profile owner lease on
    that root refuses the second ordinary startup.
- **`architecture_boundaries`: BPR-0807, not Linux-specific.** It stops at
  `Provider data crossed the core/Qt boundary in src/adapters/qt/src/workbench_window.cpp: https?://`,
  which is BPR-0807's first finding.
- **`test_network_isolation_manifest`: BPR-0802/BPR-0812, not
  Linux-specific.** It reports
  `TEST_NETWORK_ISOLATION_FAILED:raw add_test ownership is ['cmake/BprTestIsolation.cmake', 'tests/github_contract/CMakeLists.txt'], expected only cmake/BprTestIsolation.cmake`.
  That is the source-scan finding for the `if(APPLE)` raw `add_test` of
  `gh_cli_git_smart_transport`, owned by BPR-0802. On Linux that test is not
  registered, so the Mac-only boundary-marker finding does not appear.

## Limitations

- **WSL2 kernel.** WSL2 runs a Microsoft Linux kernel in a utility VM, not a
  full-system distribution kernel. Process-group, `flock` and `waitid`
  semantics here come from that kernel.
- **Offscreen registration.** `qt_agent_launch_canary` and both
  application crash courses are registered with `QT_QPA_PLATFORM=offscreen`
  and `QT_STYLE_OVERRIDE=Windows`, and ran that way unchanged. Their claims here
  are process-lifetime and lock semantics, not display behaviour. This file
  makes no WSLg display claim.
- **Supervisor binary.** The supervisor exercised here is built from
  `9bbe3604`, not byte-identical to `17f495fe`. The source difference is the
  one line above. No independent review was requested for that one-line change.
  `clang-format` 22.1.8 (`/usr/bin/clang-format`) and `git diff --check` pass
  on it. `coding-tidy` was not run.
- **Not covered here.** Windows implementation and evidence for this card
  remain open, and this Linux run does not change that.

## Logs and cleanup

- **Logs.** Build and test logs were kept outside the checkout under
  `/home/mmarcin/bpr-station-logs/task-20260917-lnx-evidence/`. They are not
  committed.
- **Temporary roots.** The contained runners consumed their own temporary
  roots.
- **Boundaries.** No GitHub or GHES request, credential read, live agent, paid
  service or GitHub Actions run was used.
