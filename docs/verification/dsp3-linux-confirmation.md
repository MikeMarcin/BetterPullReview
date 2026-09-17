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
- **State.** Steps 0 to 3 are recorded below, and all of them pass. Steps 4
  to 7 are not yet run.
- **Commands.** None is active.
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
| 4 | BPR-0820 | not yet run | | |
| 5 | portable test changes | not yet run | | |
| 6 | BPR-0835 | not yet run | | |
| 7 | BPR-0825 | not yet run | | |

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
