# dsp-004 Linux confirmation runs

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-004` (station
  dispatch `bpr-linux/int-10`), mandate r2, task
  `task-20260917-lnx-0851-sweep`, reply to `bpr-main-mac`. Station
  `bpr-linux` (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`, authority
  `lane-commit`. Envelope base `c20600af38ac603aca11f0c23c4de6999c9843de`;
  the lane started at `a1864fa296ce3450ce0e0470f36bd0d95cd59470`, which is
  main and contains the BPR-0851/0852 candidate `15eb1540`. RFC-BPR-0029 base
  `5d5df7cd79115b9b4b2f8be6ebdb5392458b9e1d`.
- **Scope.** Evidence only: the Linux runs of the [[BPR-0851]] and
  [[BPR-0852]] fix, and one read-only run of the whole suite. Both cards
  belong to main. No card, RFC or product file is edited or moved here.
- **State.** Step 1 is complete: all 7 rows pass, and no `tmp_*` file is
  left in any cache pack directory. Step 2, the full-suite sweep, is next.
- **Commands.** None is active. No child agent ran.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-0851-sweep/`
  (`step0.summary`, `configure-dev.log`, `build-dev*.log`,
  `step1.summary`, `step1-<row>.log`, `observe/`). None is committed.
- **Earlier dispatch.** The BPR-0846 runs of `bpr-linux/int-9` keep their
  binding in the next section, and their results follow it.

## Binding of the BPR-0846 runs (int-9)

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

## BPR-0851 and BPR-0852 at a1864fa2 (int-10)

### Revision and build

Station `bpr-linux` ran these on JaegAW (WSL2) on 2026-09-17, on branch
`linux-lane` at `a1864fa296ce3450ce0e0470f36bd0d95cd59470`, a clean tree.
Host, compiler, CMake, Ninja, Qt and test Python are as in the table above.
Git is `git version 2.53.0`, and `GIT_EXECUTABLE` in the cache is
`/usr/bin/git`. No `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, `LDFLAGS`, `CMAKE_*`,
`QT_*`, `CTEST_*` or `TMPDIR` variable was set.

- **Configure.** `cmake --preset dev`: rc 0, 16:42:02Z to 16:42:07Z. Stock
  preset; `CMAKE_BUILD_TYPE` is `Debug` and `CMAKE_CXX_FLAGS` is empty. It
  printed 124 `CMake Warning (uninitialized)` blocks, all inside Qt 6.10.3's
  and KSyntaxHighlighting 6.28.1's own package files under `build/`. The
  preset turns on `--warn-uninitialized`; none of the warnings is in a
  project file.
- **Build.** `cmake --build --preset dev`: rc 0, 16:42:07Z to 16:43:08Z,
  429 steps from the tree int-9 built. Then `cmake --build --preset dev
  --clean-first`, so that every translation unit's warnings would show: rc 0,
  1253 of 1253 steps, 16:43:25Z to 16:45:53Z. Neither log has a `warning:` or
  `error:` line. `ninja -n` then reported no work.

### How the rows ran

`ctest --test-dir build/dev -N` lists 366 tests. Each of the 7 names matched
exactly one row: `git_protocol_boundary` (#7), `git_process_contract` (#252),
`git_cache_contract` (#253), `git_sparse_cache_contract` (#254),
`git_haves_trimmed_pack_update` (#256),
`git_haves_trimmed_pack_missing-object` (#257) and
`git_haves_trimmed_pack_unconnected` (#258). No row was substituted.

The Secret Service `login` collection read `(<false>,)` at 16:46:18Z. Each
row then ran once, in its own serial `ctest --test-dir build/dev -j1 -V -R
'^<name>$'` invocation with its own log, from 16:46:18Z to 16:46:31Z. The
1-minute load average was 13.58 at the start, just after the clean build,
and 10.65 at the end. Every row runs inside the Linux network sandbox.

### Results

| Row | Result | ctest time | Printed |
|---|---|---|---|
| `git_haves_trimmed_pack_update` | Pass | 0.44 s | `UPDATE_SERVED_PACK_INDEX_PACK_EXIT:1`, `UPDATE_EPOCH_STATUS:6`, `haves-update-loads:pass` |
| `git_haves_trimmed_pack_missing-object` | Pass | 0.50 s | `EPOCH_CACHE_FAILURE:10:fatal: did not receive expected object <oid>`, `EPOCH_EFFECT_FAILED:revision-prepare:6`, `MISSING_SERVED_PACK_INDEX_PACK_EXIT:128`, `MISSING_REFUSAL_REASONS:27`, `haves-missing-object-refused:pass` |
| `git_haves_trimmed_pack_unconnected` | Pass | 0.18 s | `UNCONNECTED_PACK_INDEX_PACK_EXIT:1`, `UNCONNECTED_CACHE_FAILURE:10:fsck reported 1 missing commit, 1 missing blob, 2 broken links`, `CONNECTED_CACHE_FAILURE:-1`, `haves-unconnected-residency-refused:pass` |
| `git_process_contract` | Pass | 5.36 s | `Git process contract passed` |
| `git_cache_contract` | Pass | 4.62 s | `Git cache contract passed` |
| `git_sparse_cache_contract` | Pass | 1.45 s | `SPARSE_STRICT_REFUSAL:5:10:0`, `6:13:1`, `7:13:1`, `8:13:4`, `Git sparse cache contract passed` |
| `git_protocol_boundary` | Pass | 0.35 s | `Ran 18 tests`, `OK` |

What the markers show on Linux:

- **BPR-0851, the refusal.** `MISSING_REFUSAL_REASONS` is `27` alone, as on
  macOS: `27` is `epoch_reason_code::process_failure` and `26`,
  `repair_required`, does not appear. The cache failure is now `10`
  (`process_failure`) at `revision-prepare`, where int-9 saw `13` before the
  fix, and the effect failure is `6` (`epoch_failure::process_failure`). The
  course then requires that the earlier head stays latest and that a later
  valid import loads without repair; it passes.
- **BPR-0852, the detail.** `UNCONNECTED_CACHE_FAILURE` is
  `10:fsck reported 1 missing commit, 1 missing blob, 2 broken links`,
  byte-identical to the macOS value. The course requires `1 missing commit`
  and that the base commit's name is absent.
- **The sparse contract.** Course 5 refuses with `10` and no residue.
  Courses 6 to 8 refuse with `13` and leave exactly the planted names: 1, 1,
  and 4, since the non-private `tmp_idx_BPRshr` is planted only off Windows.

### No `tmp_*` file is left in the pack directories

The courses cannot keep their files. Each creates a root under `/tmp` named
`bpr-haves-<label>-<clock>`, and its destructor removes it with
`std::filesystem::remove_all`. The binary has no keep or trace option, and
this station has no `gdb`, `strace` or `inotifywait`.

**In-course check.** The missing-object course itself requires, right after
the refusal, that no file named `tmp_pack_*` or `tmp_idx_*` exists anywhere
under its cache root `<root>/git` (`index_residue`). It passed in the ctest
run above. The other two courses make no such check.

**Observation before cleanup.** Each course was then run once more, outside
ctest, with the registered command: `better_pull_review_network_sandbox`,
the course binary and `--update`, `--missing-object` or `--unconnected`,
from `build/dev/tests/git_contract` with `BPR_AUTOMATED_TEST_BOUNDARY=1`.
Added to that environment was `LD_PRELOAD` of a shim built only in the log
directory (`observe/remove_all_observer.c`, `cleanup_observer_addon.c`). The
registered row unsets `LD_PRELOAD`, so these runs could not go through
ctest. The shim changes no call. It wraps
`std::filesystem::remove_all(const path&, error_code&)` and, when the
argument's name starts with `bpr-haves-`, first walks that root. For every
`objects/pack` directory it records each entry with its type, mode and link
count, and it records any entry anywhere whose name starts with `tmp_`. A
second version also records each `syscall(SYS_renameat2, ...)` whose source
name starts with `tmp_`, and each `std::filesystem::remove` of a
`.bpr-file-cleanup-*` quarantine in a pack directory. Each version ran the
three courses once (16:46:55Z to 16:46:57Z and 16:47:52Z to 16:47:55Z). All
six runs exited 0 with the same pass markers as the ctest runs.

| Course | Cache pack directory at cleanup | `tmp_*` anywhere under the root | Cleanup calls seen |
|---|---|---|---|
| update | 4 entries: 2 `pack-*.pack` and 2 `.idx`, mode 400 | None | None |
| missing-object | 4 entries: 2 `pack-*.pack` and 2 `.idx`, mode 400 | One, in the course's own scratch repository, not the cache (below) | One: `objects/pack/tmp_pack_vijD1a` (regular, mode 400, 1 link, own uid) renamed with `RENAME_NOREPLACE` to `.bpr-file-cleanup-<hex>` (result 0), and that quarantine removed (true) |
| unconnected | 6 entries: 3 `pack-*.pack` and 3 `.idx`, mode 400 | None | Two quarantine removals, whose source names were not `tmp_`, so the shim did not log them |

- **The cache.** No run of either version found a `tmp_*` file in the
  cache's `objects/pack` directory or anywhere else under `<root>/git`. The
  missing-object run shows Git 2.53.0's `index-pack` leaving
  `tmp_pack_vijD1a` in the cache and BPR-0851's cleanup removing that exact
  file. The pass is not vacuous on this Git.
- **The scratch repository.** The only `tmp_*` file seen was
  `server/missing-check.git/objects/pack/tmp_pack_*` (mode 444, 344 bytes),
  in both versions. The course creates that bare repository and runs `git
  index-pack --stdin --strict --check-self-contained-and-connected` in it
  directly, to measure the exit code 128. The product never touches it, and
  the destructor removes it with the root.
- **The unconnected removals.** The two unlogged quarantine removals are
  probably the rollback of the refused pack's `.pack` and `.idx`; this was
  not traced.
- **After the runs.** No `/tmp/bpr-haves-*` directory remained.

### Failures

None. No row failed, timed out or printed a `FAIL:` line, so no pack
listing was needed.
