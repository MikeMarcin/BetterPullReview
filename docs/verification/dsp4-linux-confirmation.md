# dsp-004 Linux confirmation runs

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-004` (station
  dispatch `bpr-linux/int-9`), mandate r2, task
  `task-20260917-lnx-0846-0849`. Station `bpr-linux` (`stn-3f7fcc92`,
  `wsl2`), branch `linux-lane`, authority `lane-commit`. Envelope base
  `dae5d905f7d27d96e21dbe32356617788236f4b3`; the lane started at
  `e6b7193e19862ba3f0d27c3096ba30ddc0ea60cf`. RFC-BPR-0029 base
  `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d`.
- **Scope of this file.** Step 1: the Linux runs of [[BPR-0846]]'s three
  courses and the contracts named for it. That card belongs to main. This
  file is evidence only: the card is not edited or moved, and nothing in
  product code changed.
- **State.** Step 1 is complete. All 14 runs pass. The missing-object course
  ends in cache failure 13 (`repair_required`) on Linux too, as on macOS;
  that is [[BPR-0851]].
- **Commands.** None is active. No child agent ran for this step.
- **Next.** Step 2 of the task, [[BPR-0849]], is recorded on its card.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-0846-0849/`
  (`build-dev.log`, `step1.summary`, `step1-<course>-run<n>.log`). None is
  committed.

## Station, revision and build

Station `bpr-linux` ran these on JaegAW (WSL2) on 2026-09-17, on branch
`linux-lane`, a clean tree. The build is from
`e6b7193e19862ba3f0d27c3096ba30ddc0ea60cf`, and the runs are at
`458fe130776164c79a0cae9bb9154f4d3a917d9d`, which only adds the claim of
[[BPR-0849]] (a card move). Both contain
`bbdf8cc0e9d5db0ebd66b05b8f3d1fcd76a62a42`, the import rule change and the
three courses.

| Fact | Value |
|---|---|
| Git | `git version 2.53.0` (`/usr/bin/git`), the version that found this |
| Kernel | `6.18.33.2-microsoft-standard-WSL2`, x86_64 |
| Compiler | GCC 15.2.0 (`Ubuntu 15.2.0-16ubuntu1`), `/usr/bin/c++` |
| Build tools | CMake 4.4.2, Ninja 1.13.2 |
| Qt | 6.10.3 `gcc_64` from the project's dependency cache |
| Test Python | `~/.local/bin/python3` 3.13.15 |
| Environment | No `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, `CMAKE_*`, `QT_*`, `CTEST_*` or `TMPDIR` variable set |
| Secret Service | The `login` collection read `(<false>,)` before the course group |

- **Configure.** `cmake --preset dev`: rc 0, 15:58:33Z to 15:58:44Z. Stock
  preset, no cache overrides.
- **Build.** `cmake --build --preset dev`: rc 0, 15:58:44Z to 16:00:13Z. It
  compiled `installed_git_process.cpp` and
  `haves_trimmed_pack_integration_tests.cpp`, among others. The log has 0
  `warning:` and 0 `error:` lines. `ninja -n` then reported no work.

## How the rows ran

`ctest --test-dir build/dev -N` lists 366 tests. Each of the 8 names matched
exactly one row: `git_haves_trimmed_pack_update` (#256),
`git_haves_trimmed_pack_missing-object` (#257),
`git_haves_trimmed_pack_unconnected` (#258), `git_process_contract` (#252),
`git_cache_contract` (#253), `git_sparse_cache_contract` (#254),
`epoch_maintenance_integration` (#255) and
`application_epoch_maintenance_public_path` (#343). No row was substituted.

Every run was its own serial `ctest --test-dir build/dev -j1 -V -R
'^<name>$'` invocation with its own log, from 16:00:55Z to 16:01:17Z. The
1-minute load average was 3.71 at the start, just after the build, and 2.72
at the end. Every row runs inside the Linux network sandbox. Only
`application_epoch_maintenance_public_path` is offscreen by registration
(`QT_QPA_PLATFORM=offscreen`, `QT_STYLE_OVERRIDE=Windows`); the Git rows set
no Qt platform.

## Results

### The three BPR-0846 courses, 3 runs each

| Course | Run | Result | ctest time | Markers |
|---|---|---|---|---|
| `git_haves_trimmed_pack_update` | 1 | Pass, `haves-update-loads:pass` | 0.46 s | `UPDATE_SERVED_PACK_INDEX_PACK_EXIT:1`, `UPDATE_EPOCH_STATUS:6` |
| `git_haves_trimmed_pack_update` | 2 | Pass, `haves-update-loads:pass` | 0.45 s | `UPDATE_SERVED_PACK_INDEX_PACK_EXIT:1`, `UPDATE_EPOCH_STATUS:6` |
| `git_haves_trimmed_pack_update` | 3 | Pass, `haves-update-loads:pass` | 0.44 s | `UPDATE_SERVED_PACK_INDEX_PACK_EXIT:1`, `UPDATE_EPOCH_STATUS:6` |
| `git_haves_trimmed_pack_missing-object` | 1 | Pass, `haves-missing-object-refused:pass` | 0.28 s | `MISSING_SERVED_PACK_INDEX_PACK_EXIT:128`, `EPOCH_CACHE_FAILURE:13:`, `EPOCH_EFFECT_FAILED:revision-prepare:5` |
| `git_haves_trimmed_pack_missing-object` | 2 | Pass, `haves-missing-object-refused:pass` | 0.28 s | `MISSING_SERVED_PACK_INDEX_PACK_EXIT:128`, `EPOCH_CACHE_FAILURE:13:`, `EPOCH_EFFECT_FAILED:revision-prepare:5` |
| `git_haves_trimmed_pack_missing-object` | 3 | Pass, `haves-missing-object-refused:pass` | 0.27 s | `MISSING_SERVED_PACK_INDEX_PACK_EXIT:128`, `EPOCH_CACHE_FAILURE:13:`, `EPOCH_EFFECT_FAILED:revision-prepare:5` |
| `git_haves_trimmed_pack_unconnected` | 1 | Pass, `haves-unconnected-residency-refused:pass` | 0.18 s | `UNCONNECTED_PACK_INDEX_PACK_EXIT:1`, `UNCONNECTED_CACHE_FAILURE:10:`, `CONNECTED_CACHE_FAILURE:-1` |
| `git_haves_trimmed_pack_unconnected` | 2 | Pass, `haves-unconnected-residency-refused:pass` | 0.18 s | `UNCONNECTED_PACK_INDEX_PACK_EXIT:1`, `UNCONNECTED_CACHE_FAILURE:10:`, `CONNECTED_CACHE_FAILURE:-1` |
| `git_haves_trimmed_pack_unconnected` | 3 | Pass, `haves-unconnected-residency-refused:pass` | 0.18 s | `UNCONNECTED_PACK_INDEX_PACK_EXIT:1`, `UNCONNECTED_CACHE_FAILURE:10:`, `CONNECTED_CACHE_FAILURE:-1` |

What the markers mean at this revision:

- **Update.** Git 2.53's `index-pack --strict
  --check-self-contained-and-connected` exits 1 on the pack the server
  trimmed with the resident head as a have, the case BPR-0846 reproduced.
  `UPDATE_EPOCH_STATUS:6` is `epoch_status::ready`, so the update loads on
  Linux, and the course then checks that the catalog's latest live head is
  the update.
- **Missing object.** `index-pack` exits 128 on the pack the forged have
  left incomplete, and the head never loads. The final cache failure is
  `EPOCH_CACHE_FAILURE:13`, `git_cache_failure::repair_required`, with an
  empty detail, at `revision-prepare`; `5` is `epoch_failure::repair_required`.
  That is the code macOS shows, so Linux reproduces [[BPR-0851]]'s symptom.
  No `EPOCH_CACHE_FAILURE:10` line appears in these runs.
- **Unconnected.** `index-pack` alone accepts the trimmed pack over the
  sparse residency (exit 1). The cache refuses it with `10`,
  `process_failure`, before any witness, and the same pack imports once the
  history is connected (`-1`, no failure).

### The contracts, once each

| Row | Result | ctest time | Printed |
|---|---|---|---|
| `git_process_contract` | Pass | 5.37 s | `Git process contract passed` |
| `git_cache_contract` | Pass | 4.48 s | `Git cache contract passed` |
| `git_sparse_cache_contract` | Pass | 1.05 s | `Git sparse cache contract passed` |
| `epoch_maintenance_integration` | Pass | 0.96 s | `Epoch maintenance integration passed` |
| `application_epoch_maintenance_public_path` | Pass | 6.41 s | `EPOCH_MAINTENANCE_PUBLIC_PATH_CONTRACT_OK` |

- **`epoch_maintenance_integration`** also prints `EPOCH_CACHE_FAILURE:13:`
  and `EPOCH_EFFECT_FAILED:comparison-resolve:5`. The course passes; it
  deliberately deletes a whole cache and requires that a reopen returns
  `repair_required` (`tests/git_contract/epoch_maintenance_integration_tests.cpp`
  ~1160). Whether that line is the deletion case was not traced.
- **Not run here.** `git_protocol_boundary` ([[BPR-0850]]) and the other rows
  in main's macOS list were not part of this step.

## Failures

None. No run failed, timed out or printed a `FAIL:` line.

## What remains on the card

Main owns [[BPR-0846]]. With these runs, Linux has the three courses 3 of 3
each on Git 2.53.0, and the listed contracts pass. The Windows runs, the
measurement of the Windows containment reason and the critical review of the
import rule are not part of this step.
