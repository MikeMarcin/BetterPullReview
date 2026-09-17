# dsp-002 Linux confirmation runs

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  dispatch `bpr-linux/int-4`), mandate r2, task
  `task-20260917-lnx-confirm`. Station `bpr-linux` (`stn-3f7fcc92`, `wsl2`),
  branch `linux-lane`, authority `lane-commit`. Base
  `780d0a1c3d2c52932e007601d179657168ba3124`.
- **State.** Step 1 is recorded below: every confirmation course passes at the
  base except `application_anonymous_public_preview_public_path`, which printed
  `PUBLIC_PREVIEW_SMART_ACQUISITIONS:2` in 3 of 5 runs, as it does on macOS.
- **Next.** Step 2, BPR-0830: claim, move the round trip's "present on entry"
  onto `MaterialReceiptHits`, repeat the courses, negative control, review.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-confirm/`.
  None is committed.

## Station, revision and build

Station `bpr-linux` ran these on JaegAW (WSL2) on 2026-09-17, on branch
`linux-lane` at `780d0a1c3d2c52932e007601d179657168ba3124`, a clean tree. Main
had merged the previous `linux-lane` as `fe629c90`, and the base carries the
dsp-002 fixes for BPR-0826 (`d9ea1cbc`), BPR-0823 (`10f30f04`), BPR-0828 and
BPR-0829 (`5899f1d6`), BPR-0802 (`49d7df69`), BPR-0644 (`1925d0cc`,
`3ad87ee4`) and BPR-0622's Windows slices (`c94e02b1`).

| Fact | Value |
|---|---|
| Kernel | `6.18.33.2-microsoft-standard-WSL2`, x86_64 |
| Distribution | Ubuntu 26.04 LTS |
| CPU and memory | 20 logical CPUs, 62 GiB |
| Compiler | GCC 15.2.0 (`Ubuntu 15.2.0-16ubuntu1`) |
| Build tools | CMake 4.4.2, Ninja 1.13.2 |
| Qt | 6.10.3 `gcc_64` from the project's dependency cache |
| Git and test Python | Git 2.53.0; `~/.local/bin/python3` 3.13.15 |
| Secret Service | The `login` collection read `(<false>,)` before every course group |

**Stock build.** `cmake --preset dev --fresh` (rc 0, 7 s, empty
`CMAKE_CXX_FLAGS`), then `cmake --build --preset dev --clean-first`: rc 0,
1249 of 1249 steps, no `warning:` or `error:` line, 146 s, ending 11:08:32Z.
`ninja -n` then reported no work.

**Names.** `ctest --preset dev -N` lists 362 tests. Every name below is
registered exactly once (`ctest-N.txt`). `application_agent_invocation_.*`
matches eight rows: the host test and seven contracts.

**Runs.** Each group ran serially with `ctest --preset dev -j1 -V -R
'^(...)$'`, one log per group (`<group>.ctest.log`, `step1.summary`).

**Offscreen by registration.** Every Qt course here runs
`QT_QPA_PLATFORM=offscreen`, from the ctest environment or its own script, and
most also force `QT_STYLE_OVERRIDE=Windows`. None of these results is native UI
evidence. The rows with no Qt platform are `application_agent_invocation_host`,
`agent_parent_death`, `git_process_contract`, `git_cache_contract`,
`agent_workspace_ownership`, both network-isolation gates and
`application_review_ledger_host`.

## Results

| Card | Result on Linux | First failing marker | Linux-specific? |
|---|---|---|---|
| BPR-0622 (c)(d) | Pass, 15 of 15 | none | n/a |
| BPR-0802 | Pass, 5 of 5 | none | n/a |
| BPR-0826 | Pass | none | n/a |
| BPR-0823 | Pass, 3 of 3, three times | none | n/a |
| BPR-0828 | Pass | none | n/a |
| BPR-0829 | Fail 3 of 5 | `PUBLIC_PREVIEW_SMART_ACQUISITIONS:2` | No |
| BPR-0644 | Pass, 3 of 3, twice | none | n/a |

## BPR-0622, slices (c) and (d)

Group `0622`, 11:08:58Z, 96.23 s, ctest rc 0.

| Test | Result | Duration |
|---|---|---|
| `application_agent_invocation_host` | Pass | 0.01 s |
| `agent_parent_death` | Pass | 20.72 s |
| `git_process_contract` | Pass | 5.44 s |
| `git_cache_contract` | Pass | 4.66 s |
| `agent_workspace_ownership` | Pass | 0.71 s |
| `application_agent_crash_recovery` | Pass | 18.81 s |
| `application_agent_materialization_crash_recovery` | Pass | 7.17 s |
| `application_agent_invocation_composition_contract` | Pass | 10.44 s |
| `application_agent_invocation_construction_cut_contract` | Pass | 0.72 s |
| `application_agent_invocation_launcher_construction_cut_contract` | Pass | 0.71 s |
| `application_agent_invocation_gesture_cut_contract` | Pass | 1.18 s |
| `application_agent_invocation_cancellation_contract` | Pass | 8.23 s |
| `application_agent_invocation_staleness_contract` | Pass | 8.03 s |
| `application_agent_invocation_cleanup_retry_contract` | Pass | 8.08 s |
| `application_agent_invocation_workspace_diagnostic_contract` | Pass | 1.29 s |

**What this adds.** Slices (c) and (d) changed the guarded Git launch (a v2
launch specification with a working directory) and enabled the registry,
neighbourhood and startup recovery for `Q_OS_UNIX || Q_OS_WIN`. At a head that
contains `c94e02b1`, the POSIX Git contracts, both crash-recovery courses, the
ownership and parent-death courses and all eight invocation rows pass on Linux.
Slices (a) and (b) were recorded in
[the BPR-0622 Linux evidence](BPR-0622-linux-native-evidence.md).

## BPR-0802

Group `0802`, 11:10:34Z, 7.90 s, ctest rc 0.

| Test | Result | Duration | Markers |
|---|---|---|---|
| `test_network_isolation_structure_contract` | Pass | 0.79 s | `Ran 58 tests`, `OK (skipped=1)`. The skip is the Mach-O symbol probe, which runs only on `darwin`. |
| `test_network_isolation_manifest` | **Pass** | 1.21 s | `TEST_NETWORK_ISOLATION_OK:configured=362:registrations=343:backend=linux-network-namespace`. The raw `add_test` finding is gone. |
| `application_composition_capabilities_inside_boundary` | Pass | 0.33 s | `COMPOSITION_TEST_BOUNDARY:inside`, `COMPOSITION_BROWSER:recording`, `COMPOSITION_CLI_READ:unavailable` |
| `application_device_authorization_browser_handoff` | Pass | 5.34 s | `DEVICE_FLOW_ENROLLED_OK` |
| `application_poller_liveness` | Pass | 0.21 s | `github-application-boundary-tests:pass`, then `github-application-liveness-tests:pass` |

The boundary subtest line and the tick subtest both pass, so BPR-0827's tick
failure does not appear on Linux at this head. No person's application held
the default profile, so the BPR-0820 `PROFILE_ALREADY_OPEN` stop did not arise.

## BPR-0826

`application_agent_invocation_composition_contract` passes on Linux in 10.44 s
(group `0622` above). The failure the station reported earlier
(`application_session 2`, expected 1) does not occur with `d9ea1cbc`'s derived
window session.

## BPR-0823

Group `0823` ran three times: 11:10:42Z, 11:13:36Z and 11:13:51Z, each about
15.5 s, ctest rc 0 each time.

| Test | Run 1 | Run 2 | Run 3 |
|---|---|---|---|
| `application_appearance_theme_customization_contract` | Pass, 5.05 s | Pass, 5.35 s | Pass, 5.35 s |
| `application_appearance_gesture_contract` | Pass, 6.45 s | Pass, 6.85 s | Pass, 6.97 s |
| `application_appearance_store_fallback_contract` | Pass, 3.59 s | Pass, 3.34 s | Pass, 3.31 s |

Each run printed `APPEARANCE_THEME_CUSTOMIZATION_CONTRACT_OK`,
`APPEARANCE_GESTURE_CONTRACT_OK` and `APPEARANCE_STORE_FALLBACK_CONTRACT_OK`.
No run printed `DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED`.

**The gesture contract's `EPOCH_FIXTURE_SEED_FAILED` does not persist.** It
passed 3 of 3. The earlier failure (`EPOCH_FIXTURE_SEED_FAILED`,
`RUN_BUDGET_EXPIRED … stage=4`) was recorded at `ef3f782b`, on a probe tree
built with `-Wno-error`
([BPR-0816's evidence](BPR-0816-linux-gcc15-stock-build.md)). Its cause was not
bisected, so it is not named here. It cannot be reproduced at this base.

## BPR-0828

Group `0828`, 11:10:57Z, 2.65 s, ctest rc 0. `application_diff_workbench_public_path`
passes in 2.63 s. Its contiguous block matched, including
`REVIEW_LEDGER_PRESENT_ON_ENTRY:0`, `REVIEW_LEDGER_OK` and
`DIF_PREFERENCES:shown:system:reduced`.

## BPR-0829

Five separate `ctest` runs of `application_anonymous_public_preview_public_path`,
11:11:00Z to 11:11:04Z.

| Run | Result | Duration | `PUBLIC_PREVIEW_SMART_ACQUISITIONS` |
|---|---|---|---|
| 1 | Fail | 0.79 s | 2 |
| 2 | Fail | 0.78 s | 2 |
| 3 | Pass | 0.74 s | 1 |
| 4 | Pass | 0.77 s | 1 |
| 5 | Fail | 0.86 s | 2 |

- **First failing marker.** Every failing run printed
  `PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:Incomplete history · ambiguous occurrences
  · unresolved local records · display order is not push order`, then
  `PUBLIC_PREVIEW_SMART_ACQUISITIONS:2`, then `PUBLIC_PREVIEW_ENABLED_OK`. The
  pass expression wants `:1`, so ctest reported the expression not found.
  Every other line in the block matched, including the newly named history
  line and `PUBLIC_PREVIEW_OCCURRENCES:4`.
- **Not Linux-specific.** The card records `:2` on macOS at the integrated head.
- **Cause.** Not diagnosed here. The card's next step, finding which operations
  perform the two acquisitions, is main's.

## BPR-0644

Group `0644` ran twice: 11:11:04Z (56.41 s) and 11:12:00Z (56.13 s), ctest rc 0
both times.

| Test | Run 1 | Run 2 | Marker |
|---|---|---|---|
| `application_device_binding_restart` | Pass, 28.93 s | Pass, 28.95 s | `device-binding-restart-tests:pass` |
| `application_device_binding_launch_renewal` | Pass, 19.84 s | Pass, 19.63 s | `device-binding-launch-renewal-tests:pass` |
| `application_device_binding_relaunch` | Pass, 7.63 s | Pass, 7.54 s | `DEVICE_BINDING_RELAUNCH_OK` |

**The quit and SIGTERM phases, observed.** The relaunch driver prints only its
final marker. It was run once more inside the same network sandbox and
environment, through a wrapper that logs each phase's exit status and whether
its pass marker appeared. It prints no vault material. It exited 0 in 7.58 s
(`0644-relaunch-observe.log`):

```text
OBSERVED_PHASE seed rc=0 pass_marker=True
OBSERVED_PHASE quit rc=0 pass_marker=True
OBSERVED_PHASE quit rc=0 pass_marker=True
OBSERVED_PHASE terminate signal=SIGTERM child_returncode=-15
OBSERVED_PHASE verify rc=0 pass_marker=True
OBSERVED_PHASE locked rc=0 pass_marker=True
DEVICE_BINDING_RELAUNCH_OK
```

On Linux the terminated child died of SIGTERM with no shutdown (return code
-15). The relaunch after it restored the live binding, and so did both orderly
quits. These are regression courses and, as the card says, do not reproduce
the original observation. The vault is the course's synthetic fixture vault,
not Secret Service. Criterion 3 (attended, macOS) and the owner question are
untouched.

## Ledger courses at the base, before BPR-0830

Group `0830-base`, 11:12:56Z, 5.93 s, ctest rc 0: `application_review_ledger_host`
(0.00 s), `application_review_ledger_composition_contract` (4.35 s) and
`application_review_round_trip_composition_contract` (1.56 s, with the pins as
committed) pass.
