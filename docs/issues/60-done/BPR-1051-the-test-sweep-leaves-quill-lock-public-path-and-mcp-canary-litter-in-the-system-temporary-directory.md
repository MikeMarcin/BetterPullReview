---
schema: "implement-rfc.card.v1"
id: "BPR-1051"
title: "The test sweep leaves QuillBackendLock*, bpr-public-path-* and bpr-mcp-canary-* litter in the system temporary directory"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, the station's shared /tmp during the 2026-09-23 runs","counted_at":"d4f9d07d","related_cards":["BPR-1047","BPR-1044"],"reservation":"docs/issues/_reservations/BPR-1050--BPR-1051--e733678668d2.json"}
acceptance: [{"criterion":"AC1 Each of the three kinds (five, under the amendment) is attributed to the rows that leave it, with the measurement that shows it","outcome":"pass","revision":"ff9bc9e2","evidence":{"kind":"command","detail":"bpr-main-linux: each of the 423 rows alone (ctest -j1 -V), each with a new empty mode-0700 TMPDIR and /tmp listed before and after. QuillBackendLock<pid>: the agent crash-recovery (1), materialization crash-recovery (2), attended launch, runtime_process_surface, device-binding relaunch, profile activation and owner admission courses, each ending an application with a signal. bpr-public-path-*: application_attended_launch_contract's outlive launch. bpr-mcp-canary-*: application_mcp_context_helper. bpr<8>/template: rfc0017_public_preview_decision_contract. bpr-tidy-db-*: tidy.py.","path":"docs/issues/60-done/BPR-1051-the-test-sweep-leaves-quill-lock-public-path-and-mcp-canary-litter-in-the-system-temporary-directory.md#ac1-which-rows-leave-each-kind"}},{"criterion":"AC2 Linux: a serial offscreen sweep of the rows AC1 names passes; its TMPDIR holds no bpr-public-path-* or bpr-mcp-canary-* entry afterwards, and /tmp holds no QuillBackendLock<pid> for a pid the sweep started","outcome":"pass","revision":"09f3d48f","evidence":{"kind":"command","detail":"bpr-main-linux at 09f3d48f: the 9 named rows plus quill_logging, quill_logging_backpressure and application_logging, each alone with its own TMPDIR, pass 12 of 12, every TMPDIR empty and /tmp unchanged. The whole 423-row suite with TMPDIR unset: 418 of 423 (the five known red rows), and /tmp gained no entry owned by the user.","path":"docs/issues/60-done/BPR-1051-the-test-sweep-leaves-quill-lock-public-path-and-mcp-canary-litter-in-the-system-temporary-directory.md#ac2-and-ac3-linux"}},{"criterion":"AC3 Negative control: undoing each fix brings its entry back in the same measurement","outcome":"pass","revision":"09f3d48f","evidence":{"kind":"command","detail":"bpr-main-linux: quill_log.cpp, the canary, the attended launch course, the rfc0017 test and tidy.py reverted to 5b4d1dd7 and rebuilt: /tmp gained 8 QuillBackendLock<pid> from the same seven rows, the TMPDIRs gained bpr-mcp-canary-*, bpr-public-path-* and bpr<8>, quill_logging failed 'a running backend creates no Quill lock file in /tmp', and tidy.py left bpr-tidy-db-*. Restored to the candidate's SHA-256 and rebuilt. macOS repeated the Quill control: the check restored leaves /private/tmp/QuillBackendLock30415 under the runner's sandbox.","path":"docs/issues/60-done/BPR-1051-the-test-sweep-leaves-quill-lock-public-path-and-mcp-canary-litter-in-the-system-temporary-directory.md#ac2-and-ac3-linux"}},{"criterion":"AC4 macOS: the same measurement over the same rows by a direct sandboxed run of each row's command, not through ctest; the Quill lock checked in /tmp, recording whether the sandbox let the process create it","outcome":"pass","revision":"bcd57840","evidence":{"kind":"command","detail":"bpr-main-mac at bcd57840: 12 of 12 rows, all registered on macOS, run directly under the runner's own derived profile with a kept TMPDIR, pass; none of the five kinds left and no new QuillBackendLock in /private/tmp; through ctest 12 of 12. The sandbox allows the lock: with the singleton check restored the attended launch course leaves /private/tmp/QuillBackendLock30415 and quill_logging fails; restored (SHA-256 c7c588af) and rebuilt.","path":"docs/issues/60-done/BPR-1051-the-test-sweep-leaves-quill-lock-public-path-and-mcp-canary-litter-in-the-system-temporary-directory.md#macos-ac4-bpr-main-mac-2026-09-24"}},{"criterion":"AC5 Windows: the same measurement with %TEMP% pointed at an empty directory; there is no Quill lock file on Windows","outcome":"pass","revision":"bcd57840","evidence":{"kind":"command","detail":"bpr-main-win, build/windows-dev MSVC 19.51 Debug at bcd57840, the card's loop (TEMP and TMP set to a new empty directory per row, listed recursively afterwards): the 11 rows are registered here (quill_logging_backpressure is UNIX-only). Uncontained and passing, each leaving nothing: rfc0017_public_preview_decision_contract (in a short directory; the card's long name overran MAX_PATH in its scratch git, which ignores core.longpaths), application_mcp_context_helper, both agent crash-recovery courses, application_attended_launch_contract, runtime_process_surface, application_device_binding_relaunch, application_profile_activation_public_path (run after foreground clearance, 13.44 s), quill_logging and application_logging. application_profile_owner_admission_public_path is contained and fails in BPR-0907's known way; its container AC\\Temp was empty.","path":"docs/issues/60-done/BPR-1051-the-test-sweep-leaves-quill-lock-public-path-and-mcp-canary-litter-in-the-system-temporary-directory.md#windows-ac5-ten-of-eleven-rows-measured-bpr-main-win-2026-09-24"}},{"criterion":"AC6 application_mcp_context_helper removes no path it did not create","outcome":"pass","revision":"09f3d48f","evidence":{"kind":"command","detail":"The canary's scratch is bpr-mcp-canary-<random 64-bit> created with create_directory; an existing name is passed over, never removed; its only removal is of the directory create_directory reported it created. The pre-fix source's remove_all of a start-time-derived name is gone. The row passes on Linux, macOS and Windows and leaves nothing.","path":"docs/issues/60-done/BPR-1051-the-test-sweep-leaves-quill-lock-public-path-and-mcp-canary-litter-in-the-system-temporary-directory.md#the-other-fixes"}}]
---

# BPR-1051: The test sweep leaves QuillBackendLock*, bpr-public-path-* and bpr-mcp-canary-* litter in the system temporary directory

## Defect

Test runs on the Linux station leave three kinds of entry in the shared
temporary directory, and nothing removes them. They build up run after run.
[[BPR-1047]] is the same kind of defect for `bpr-ledger-preflight-*`. That
card owns that name, and this card owns these three.

## Counted (bpr-main-linux, 2026-09-24)

Station `bpr-main-linux` (WSL2, kernel `6.18.33.2-microsoft-standard-WSL2`),
at `d4f9d07d`. `/tmp` is tmpfs, mode `1777`, and `TMPDIR` is unset. The
station has been up since 2026-09-17. An earlier count on the same station
found 109 lock files. At the time of writing there were:

| Entry | Count | Shape | Dates |
| --- | ---: | --- | --- |
| `/tmp/QuillBackendLock<pid>` | 105 | empty regular files, mode `0644` | 20 on 2026-09-21 (21:07 to 21:12), 85 on 2026-09-23 (08:00 to 20:49) |
| `/tmp/bpr-public-path-XXXXXX` | 10 | directories, mode `0700`, 592 KiB each, each holding `data/` and `data-agent-workspaces-v1/` | 2026-09-23 18:50 to 19:08 |
| `/tmp/bpr-mcp-canary-<seconds>` | 2 | directories, mode **`0755`**, 136 KiB each: numbered snapshots, captured standard error and `requests.jsonl` | 2026-09-23 09:53 and 18:57 |

All of them are owned by the station user. This card does not remove any of
them. They were found in a directory that other processes share, and this
card's evidence does not prove which run made each one.

## Causes, read from the code at `d4f9d07d`

### `QuillBackendLock<pid>`: Quill's backend lock, left by a process that ends abnormally

- Quill v13.0.0 (`cmake/BprQuillProvider.cmake`, header-only) starts a
  `BackendWorkerLock` when `BackendOptions::check_backend_singleton_instance`
  is true, which is the default. On POSIX it opens
  `"/tmp/QuillBackendLock" + pid` with `O_CREAT`, mode `0644`, and takes a
  `flock` (`quill/backend/BackendWorkerLock.h`).
- The path is a literal `/tmp`, so `TMPDIR` does not move it. On Windows the
  lock is a named mutex, `Local\QuillBackendLock<pid>`, and no file is
  created.
- The file is unlinked only in the lock's destructor, which runs when the
  backend stops. `src/adapters/logging/src/quill_log.cpp`'s `backend_lease`
  calls `quill::Backend::stop()` when its last user releases, and Quill's
  `BackendWorker` destructor stops it at a normal exit.
- So the file stays whenever a process that has started the logging backend
  ends without stopping it: it is killed, it crashes or aborts, or it leaves
  through `_exit`. Quill's own comment calls this harmless because `/tmp` is
  cleared at reboot.
- Courses that end a composed application on purpose are the likely
  producers. This has not been measured. The candidates are the drivers
  that kill or terminate a phase, such as
  `device_binding_relaunch_contract.py`, `profile_owner_admission_contract.py`,
  `profile_activation_contract.py`, `attended_launch_contract.py` and
  `release_security_course.py`, and the agent crash-recovery courses.
- The same file is left by the **shipped application** on Linux and macOS
  whenever it is killed or crashes. That is product behaviour, not only a
  fault of the tests.

### `bpr-public-path-XXXXXX`: the fixture's own scratch data root

- `src/app/src/github_application.cpp:13396-13400`, under
  `BPR_BUILD_TESTING`: when a fixture start has no `ApplicationDataRoot`, the
  startup implementation makes a `QTemporaryDir` named `bpr-public-path-XXXXXX`
  in `QDir::temp()`, roots the data at its `data` child, and keeps it as
  `Scratch`.
- It is removed only when that implementation is destroyed. The leftovers
  hold a populated `data/` and the agent workspace neighbourhood, so the
  application ran and did not tear the startup implementation down. Either
  the process ended abnormally or the implementation outlived the exit.
- All ten are from one 18-minute window. That fits a single sweep, but which
  rows made them has not been measured.

### `bpr-mcp-canary-<seconds>`: a scratch directory no one removes

- `tests/application/mcp_context_helper_tests.cpp:78-94`
  (`application_mcp_context_helper`) makes
  `temp_directory_path() / ("bpr-mcp-canary-" + time(nullptr))` once per
  process, and nothing removes it. Every run leaves one.
- Before creating it, the test calls `std::filesystem::remove_all` on that
  name. Two runs that start in the same second, from two build trees for
  example, would delete each other's scratch. That breaks the rule that a
  test removes only what it created.
- It is created with default permissions (`0755` here), not owner-private.

## Scope

- Measure which rows leave each kind of entry, before changing anything.
  - `bpr-public-path-*` and `bpr-mcp-canary-*` follow `TMPDIR`. Point `TMPDIR`
    at a new, empty, owner-private directory for a serial sweep, and read it
    afterwards. This works through ctest on Linux, whose sandbox leaves
    `TMPDIR` alone, and on Windows. It does not work through ctest on macOS
    (AC4).
  - Quill's lock ignores `TMPDIR`. Snapshot `/tmp` before and after each row
    on a quiet station, or record the pids the row starts. Then attribute each
    new lock file by its pid.
- `bpr-mcp-canary-*`: give the canary a scratch directory it creates
  uniquely and owns. Make it owner-private, and remove it when the test
  finishes, whether the test passes or fails. Do not `remove_all` a path
  that the test did not create.
- `bpr-public-path-*`: find why the scratch outlives the process in the rows
  that leave it. Fix the teardown. Where a course kills the application on
  purpose, pass it a data root the course owns and removes. Do not add a
  test-only seam to the product for this.
- `QuillBackendLock*`: decide with evidence, and record the decision here.
  - The course that kills a process removes that pid's lock file by its exact
    name, `"/tmp/QuillBackendLock" + pid`. It knows the pid, so it can prove
    the file is its own.
  - Or the product sets `check_backend_singleton_instance = false`. The lock
    only detects Quill compiled into two binary modules of one process. Say
    whether this build can produce that. This choice would also stop the
    shipped application leaving the file after a crash.
  - Or a combination. A product change is also a logging-adapter change, and
    must pass `quill_logging` and the logging rows on all three platforms.
- Leave the existing litter to the station operators. A cleanup removes only
  what a run is proven to have made, and removes it by exact name.

## Acceptance

- AC1 Each of the three kinds is attributed to the rows that leave it, with
  the measurement that shows it.
- AC2 Linux: a serial offscreen sweep of the rows that AC1 names passes.
  Its `TMPDIR` holds no `bpr-public-path-*` or `bpr-mcp-canary-*` entry
  afterwards, and `/tmp` holds no `QuillBackendLock<pid>` for a pid that the
  sweep started, unless this card records why that entry must stay.
- AC3 Negative control: undoing each fix brings its entry back in the same
  measurement.
- AC4 macOS: the same measurement over the same rows, made by a direct
  sandboxed run of each row's command, **not through ctest**. Under ctest,
  `scripts/run_network_isolated_test.py` points `TMPDIR` at
  `/tmp/bpr-net-*/fixture`, a mode-`0700` directory that the runner deletes
  when the row ends, so a ctest run cannot show a leftover. The direct run
  uses the same sandbox profile, with `TMPDIR` pointed at an empty directory
  that is kept and read afterwards. The Quill lock is checked in `/tmp`, and
  the record says whether the sandbox let the process create it.
- AC5 Windows: the same measurement with `%TEMP%` pointed at an empty
  directory. There is no Quill lock file on Windows.
- AC6 `application_mcp_context_helper` removes no path it did not create.

## Evidence

- The counts, shapes and dates above were read on bpr-main-linux with
  `find /tmp -maxdepth 1 -name … -printf`, at `d4f9d07d`, without opening,
  moving or removing any entry except to list two directories' top level.
- The Quill mechanism was read from the staged
  `build/dev/_bpr/quill/quill-13.0.0/include/quill/backend/BackendWorkerLock.h`
  and `BackendWorker.h` (`run`, `stop`).

## One sweep's litter, observed (bpr-main-linux, 2026-09-24)

While verifying [[BPR-1048]], this station listed the top level of `/tmp`
before and after its own runs. Another session's tests on this station had
finished by 21:42. None was seen running during these runs, and every new
entry is dated inside them. These are observations, not AC1's measurement. The listings are
station records under `build/logs/bpr-main-linux-20260924/bpr-1048/`
(`tmp-*.txt`) and are not committed.

- **A serial sweep of the 156 `application_` rows** (`TMPDIR` unset,
  21:59-22:19) left seven `QuillBackendLock<pid>` files, one
  `bpr-public-path-*` and one `bpr-mcp-canary-*`. The lock files fall in
  the time windows of the agent crash-recovery courses, the attended launch
  contract, the device-binding relaunch course and the profile
  activation, admission and window-restart courses. The windows are
  estimated from row durations, so each is approximate. The
  `bpr-public-path-*` directory was created in the same second as the
  attended launch contract's lock file.
- **Eight rows in isolation** (`application_github_host`, three device
  authorization rows, `sqlite_device_binding_storage` and the three
  device-binding courses) left exactly one lock file, dated during
  `application_device_binding_relaunch`, the last row. That row ends its
  application with `terminate` on purpose.
- **A fourth kind, outside this card's title.** `_coding-standard/scripts/tidy.py`
  writes a filtered `compile_commands.json` into
  `tempfile.mkdtemp(prefix="bpr-tidy-db-")` (`tidy.py:122`) whenever it
  strips flags the analyser cannot take, which GCC's module flags always
  trigger here. Nothing removes the directory. `/tmp` held six, one per run.
  The owner of this card may take it here or card it apart. The coordinator
  has been told.

Each entry these runs left was removed by exact name: one lock file from an
earlier 17-row run that also included the relaunch course, nine entries from
the sweep, one lock file from the eight-row run, and the one
`bpr-tidy-db-*` directory made by this station's own tidy run. The entries
from before these runs are left alone.

## Amendment: two more kinds of the same litter (bpr-main-linux, 2026-09-24)

This card's owner folded two more kinds into its scope. The criteria above
now apply to all five kinds:

- **`bpr-tidy-db-*`**, left by `_coding-standard/scripts/tidy.py`. It is
  recorded above under "A fourth kind", and the coordinator asked for it to be
  taken here.
- **`bpr` followed by 8 random characters, holding `template/`.** AC1's
  measurement found it, left by `rfc0017_public_preview_decision_contract`.
  It is the same defect, and the fix is a few lines in the test that leaves
  it, so it is taken here and not carded separately.

The criteria's wording is unchanged. AC1 to AC3 now cover five kinds, and AC4
and AC5 cover the rows that leave them wherever those rows are registered.

## Attribution, fixes and the Linux runs (bpr-main-linux, 2026-09-24)

Station `bpr-main-linux` (WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, `build/dev`
Debug, `-Werror`). The card was claimed at `5b4d1dd7`, whose code is
`ff9bc9e2`. AC1 was measured at that revision, and the change landed as
**`09f3d48f`**. Every run was serial and offscreen: `QT_QPA_PLATFORM=offscreen`,
`DISPLAY` and `WAYLAND_DISPLAY` unset, and a fresh `HOME` and fresh XDG data,
config, cache, state and runtime directories, which were removed afterwards.
The logs are station records under
`build/logs/bpr-main-linux-20260924/bpr-1051/` and are not committed.

### What `/tmp` held before this station's runs

`/tmp` was counted at 06:50Z, before this station ran anything. It held:

- 106 `QuillBackendLock*` files;
- 10 `bpr-public-path-*` directories;
- 2 `bpr-mcp-canary-*` directories;
- 5 `bpr-tidy-db-*` directories;
- 4 directories named `bpr` followed by 8 characters, dated 2026-09-23 from
  09:50 to 18:57.

This station touched none of them. The counts were the same after every run
below, because each entry that this station's own runs made was removed by
exact name (listed at the end).

### AC1: which rows leave each kind

**Method.** Each of the 423 configured rows ran alone with `ctest -j1 -V
-R '^<row>$'`, from 07:42:52Z to 08:18:45Z. `TMPDIR` was a new, empty,
mode-`0700` directory for each row, and the top level of `/tmp` was listed
before and after each row. 418 rows passed. The five failures are the known
red rows:

- `qt_anonymous_public_preview_scale_100`, `_125` and `_200` ([[BPR-1023]]);
- `application_agent_finding_dif_context_contract` ([[BPR-0966]]);
- `application_uiforetw_selected_diff_contract` ([[BPR-1036]]).

The only other new `/tmp` entries were root-owned `systemd-private-*`
directories, which system services make.

An earlier attempt at this sweep passed the fresh `HOME` and XDG roots as
relative paths, which broke `documentation_structure` and two other rows. It
was stopped after 146 rows and does not count. It had already seen the
`rfc0017` and canary entries.

| Kind | Where | Rows that left it | Why |
| --- | --- | --- | --- |
| `QuillBackendLock<pid>` | `/tmp`, whatever `TMPDIR` says | `application_agent_crash_recovery` (1), `application_agent_materialization_crash_recovery` (2), `application_attended_launch_contract` (1), `runtime_process_surface` (1), `application_device_binding_relaunch` (1), `application_profile_activation_public_path` (1), `application_profile_owner_admission_public_path` (1) | Each course ends an application process with a signal on purpose, so the logging backend never stops. That makes 8 per sweep, and [[BPR-1050]]'s 164-row sweep with `TMPDIR` unset left the same 8. The profile window restart and release security courses, which the card suspected, left none |
| `bpr-public-path-XXXXXX`, mode `0700`, holding `data/` and `data-agent-workspaces-v1/` | `TMPDIR` | `application_attended_launch_contract` | Its outlive launch passes no data root, so the fixture makes its own scratch root, and the course ends the application with `terminate()`. The startup implementation that owns the scratch is never destroyed. The other courses that kill an application pass their own roots |
| `bpr-mcp-canary-<seconds>`, mode `0755` | `TMPDIR` | `application_mcp_context_helper` | Nothing removes it |
| `bpr` followed by 8 characters, holding `template/` | `TMPDIR` | `rfc0017_public_preview_decision_contract` | The module keeps one `ContainedTemporaryDirectory` for its shared template, and nothing cleans it up |
| `bpr-tidy-db-*`, holding `compile_commands.json` | `TMPDIR` | `tidy.py`, which is not a ctest row | [[BPR-1050]]'s tidy run with `TMPDIR` pointed at a kept directory left `bpr-tidy-db-5aj08q8y` |

### The Quill lock: the product turns the check off

**Decision:** `src/adapters/logging/src/quill_log.cpp` sets
`check_backend_singleton_instance = false`. No course removes lock files.

- **The check cannot fire in this build.** Quill's `BackendWorkerLock` detects
  one thing: Quill compiled into two binary modules of one process. Quill is
  included by exactly one translation unit, `quill_log.cpp`, which is
  compiled into the `STATIC` library `better_pull_review_adapter_logging`.
  The project declares no `SHARED` or `MODULE` library. So no process can hold
  two copies of Quill's backend.
- **The check's only effect was the lock.** On Linux and macOS,
  `BackendWorker::run` creates `/tmp/QuillBackendLock<pid>` synchronously
  inside `Backend::start`, and only a clean stop removes it. On Windows it is
  a named mutex, `Local\QuillBackendLock<pid>`, and the kernel releases it at
  exit.
- **Removing it course by course would miss cases.** It would take seven
  drivers in Python and C++. The shipped application would still leave the
  file after every crash or kill on Linux and macOS, and this change stops
  that too.
- **`quill_logging` now asserts the lock is gone.** On POSIX it checks that no
  `/tmp/QuillBackendLock<getpid()>` exists while its backend runs.

### The other fixes

| File | Change |
| --- | --- |
| `tests/application/mcp_context_helper_tests.cpp` | The scratch directory is `bpr-mcp-canary-<random 64-bit>`, created with `create_directory`. A name that exists is passed over, never removed. It is narrowed to `0700` on POSIX. `main` removes it and asserts that it is gone before the pass marker, and the function-local static's destructor removes it when an `expect` leaves through `std::exit`. The old `remove_all` of a name the test had not created is gone (AC6). On Windows the directory keeps the per-user `%TEMP%` entries |
| `tests/end_to_end/attended_launch_contract.py` | The outlive launch passes `--epoch-fixture-root <owned>/data --epoch-fixture-phase single`. The root is a `contained_temporary_directory`, narrowed with `make_root_private`, with the data root one owned level down ([[BPR-1050]]). The directory is removed after the process has ended. The product is unchanged, so no pin needed resealing |
| `tests/scripts/validate_rfc0017_public_preview_decision_tests.py` | `tearDownModule` removes the shared template once, after the last test |
| `_coding-standard/scripts/tidy.py` | A `finally` removes the rewritten compilation database |
| `src/adapters/logging/src/quill_log.cpp`, `tests/logging/quill_logging_tests.cpp` | As described above |

### AC2 and AC3 (Linux)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | Exit 0, no `warning:` line |
| AC2 at the candidate: the 9 rows AC1 names, plus `quill_logging`, `quill_logging_backpressure` and `application_logging`, each alone with its own new `TMPDIR` | 12 of 12 passed. `rfc0017` ran its 44 tests, and the canary and attended launch printed their pass markers. **Every `TMPDIR` was empty afterwards, and `/tmp` gained no entry** |
| AC3, the negative control. `quill_log.cpp`, the canary, the attended launch course, the `rfc0017` test and `tidy.py` were reverted to `5b4d1dd7` and rebuilt. `quill_logging`'s new assertion was kept | Every kind came back. `/tmp` gained 8 `QuillBackendLock<pid>` files, from the same seven rows. The `TMPDIR`s gained `bpr-mcp-canary-1790238149` (`0755`), `bpr-public-path-uwFusH` and `bprmzvaqbdc`. **`quill_logging` failed** with `a running backend creates no Quill lock file in /tmp`. As any failing `expect` in that test does, the failure left its `qt_temp-*` in its `TMPDIR` |
| AC3 for `tidy.py`, reverted, on `quill_logging_tests.cpp` with a kept `TMPDIR` | It left `bpr-tidy-db-8fjbc08z/compile_commands.json` |
| Restored | The SHA-256 of all six files matched the candidate again, and the tree was rebuilt |
| `tidy.py` at the candidate, on the three changed translation units, with a kept `TMPDIR` | `clang-tidy passed.` with no warning, and the `TMPDIR` was empty afterwards |
| AC6 | The canary's only removal is of the directory that `create_directory` reported it created. The negative control's source had the `remove_all` of a name derived from the start time |
| Integration, both cards: all 423 rows at `09f3d48f`, serially, with `TMPDIR` unset, so everything went to `/tmp` (08:26:07Z to 09:01:41Z) | 418 of 423 passed, in 2,134.42 s. The five failures are the known red rows listed under AC1. **`/tmp` gained no entry owned by this user.** Its one new entry is a root-owned `systemd-private-*-wsl-pro.service-*` directory, which a system service made. The log has one `AGENT_WORKSPACE_RECOVERY_REFUSED` line, from profile management's `shared-parent` section ([[BPR-1050]]) |

### Entries this station removed, each by exact name

All of them were owned by this user. Each was created inside the window of one
of this station's own runs, and each was listed before it was removed.

- From [[BPR-1050]]'s 164-row sweep (07:00:36Z to 07:20:10Z): 8
  `QuillBackendLock<pid>` files, `bpr-mcp-canary-1790233393` and
  `bpr-public-path-SeMaKa`.
- From AC1's measurement: 8 `QuillBackendLock<pid>` files, and the
  measurement's own `/tmp/b1051-*` roots.
- From AC3's negative control: 8 `QuillBackendLock<pid>` files, and its root.
- The `tidy.py` runs' own `/tmp/b1051-tidy-*` roots.

### Acceptance state

- **AC1: pass.** Each of the five kinds is attributed above, from a per-row
  measurement over all 423 rows.
- **AC2: pass on Linux.** The named rows pass and leave nothing in `TMPDIR` or
  `/tmp`. The whole 423-row sweep at `09f3d48f`, with `TMPDIR` unset, added nothing of this user's to `/tmp`.
- **AC3: pass.** Undoing the fixes brought back every kind.
- **AC4: owed by bpr-main-mac,** through the direct sandboxed run below.
- **AC5: owed by bpr-main-win.**
- **AC6: pass.**
- The logging-adapter change must also pass `quill_logging` and the logging
  rows on macOS and Windows. Those runs are included in the commands below.

### Still owed: the macOS run (bpr-main-mac), AC4

A ctest run on macOS cannot show a leftover. `scripts/run_network_isolated_test.py`
hands each row `/tmp/bpr-net-*/fixture` and deletes it when the row ends. So
each row's command runs directly, under the same sandbox template, with
`TMPDIR` pointed at a directory that is kept. None of these rows declares a
loopback fixture, so the template alone confines each one at least as tightly
as the runner's derived profile.

On a head that contains `09f3d48f`, rebuilt, for each of
`rfc0017_public_preview_decision_contract`, `application_mcp_context_helper`,
`application_agent_crash_recovery`,
`application_agent_materialization_crash_recovery`,
`application_attended_launch_contract`, `runtime_process_surface`,
`application_device_binding_relaunch`,
`application_profile_activation_public_path`,
`application_profile_owner_admission_public_path`, `quill_logging`,
`quill_logging_backpressure` and `application_logging` that is registered on
macOS:

```sh
repo=/Users/jaeger/Documents/GitHub/BetterPullReview   # the checkout
row=application_mcp_context_helper            # each row in turn
ctest --preset dev -N -V -R "^${row}\$" | grep -E 'Test command|Working Directory|Environment'
# The command is everything after the runner's `--`, and the working
# directory and ENVIRONMENT entries are the row's own.
t=$(mktemp -d /tmp/bpr1051-direct-XXXXXX)
ls /tmp | grep -c '^QuillBackendLock' > "$t.before"
cd "<working directory>"
env <ENVIRONMENT entries> BPR_AUTOMATED_TEST_BOUNDARY=1 TMPDIR="$t/" \
    /usr/bin/sandbox-exec -f "$repo/cmake/sandbox/macos-network-deny.sb" <command…>
echo "exit=$?"
find "$t" -mindepth 1 -maxdepth 2
ls /tmp | grep -c '^QuillBackendLock'; cat "$t.before"
rm -rf "$t" "$t.before"
```

Expect:

- **Each row passes**, and `find` prints nothing.
- **No new `QuillBackendLock*` appears in `/tmp`.** With the check off, Quill
  never opens the file.
- **Record whether the sandbox lets the process create that file.** One
  course, `application_attended_launch_contract`, run once with
  `options.check_backend_singleton_instance = false;` removed locally and the
  tree rebuilt, answers it. Either a `/tmp/QuillBackendLock<pid>` appears, or
  the open is denied and Quill skips the check. Record which, remove any file
  that run made by its exact name, and restore the line.
- **Record any row that is not registered on macOS.**

### Still owed: the Windows run (bpr-main-win), AC5

There is no Quill lock file on Windows. The measurement is the temporary
directory. On a head that contains `09f3d48f`, rebuilt:

```powershell
cmake --build --preset windows-dev
$rows = "rfc0017_public_preview_decision_contract","application_mcp_context_helper","application_agent_crash_recovery","application_agent_materialization_crash_recovery","application_attended_launch_contract","runtime_process_surface","application_device_binding_relaunch","application_profile_activation_public_path","application_profile_owner_admission_public_path","quill_logging","application_logging"
$savedTemp = $env:TEMP; $savedTmp = $env:TMP
foreach ($row in $rows) {
    $t = Join-Path $savedTemp ("bpr1051-" + [guid]::NewGuid())
    New-Item -ItemType Directory $t | Out-Null
    $env:TEMP = $t; $env:TMP = $t
    ctest --preset windows-dev -j1 -V -R "^$row$"
    $env:TEMP = $savedTemp; $env:TMP = $savedTmp
    "$row leaves:"; Get-ChildItem -Force $t | Select-Object Name, Mode
    Remove-Item -Recurse -Force $t
}
```

Expect:

- Each row that is registered on Windows passes, and leaves nothing in its
  directory.
- Record any row that is not registered on Windows.
- Record any row that is contained, whose launcher gives it another
  temporary directory, because this measurement cannot see what that row
  leaves.
- `quill_logging` is the logging-adapter row the change must pass on Windows.

## macOS, AC4 (bpr-main-mac, 2026-09-24)

Station `bpr-main-mac`: macOS 26.6.2 on Apple Silicon, Apple clang 21.0.0,
Qt 6.11.2, `build/dev` Debug. The head was `bcd57840`, which contains
`09f3d48f`. It was reconfigured and rebuilt, with exit 0 and no `warning:`
line. The logs, the per-row JSON and the driver are station records under
`build/logs/bpr-1050-1051-1073-1074-mac/` and are not committed.

### Method

Each row's command ran directly, not through ctest, from 09:24:30Z to
09:26:45Z. A scratch driver, not committed, did this for one row at a time:

- It read the row from `ctest --show-only=json-v1`: the command after the
  runner's `--`, the working directory, `ENVIRONMENT`,
  `ENVIRONMENT_MODIFICATION`, `TIMEOUT` and the pass and fail expressions.
- It made the runner's own layout, but kept it:
  `/private/tmp/bpr1051-direct-XXXXXX/`, mode `0700`, with a `fixture/`
  child, mode `0700`, as `TMPDIR`. It wrote `profile.sb` beside it with the
  runner's own `write_profile()`, which it imported from
  `scripts/run_network_isolated_test.py`, with no loopback ports. So each row
  ran under the profile that ctest gives it. That is the template plus the
  runner's one allowance, for sockets under its own directory. On macOS,
  `profile_activation.cpp` puts the activation endpoint in `$TMPDIR`. The
  template alone would confine the row more tightly than ctest does.
- It gave the child the row's environment, including
  `BPR_AUTOMATED_TEST_BOUNDARY=1` and the unset loader variables. It also set
  the runner's four variables: an empty `BPR_TEST_LOOPBACK_PORTS`, and
  `BPR_TEST_SOCKET_ROOT` and `TMPDIR` both naming the kept `fixture/`, with a
  new `BPR_TEST_SOCKET_TOKEN`. It then ran
  `/usr/bin/sandbox-exec -f <profile> <command…>`.
- It judged the row by CTest's rule on the captured output.
- It listed `fixture/` two levels deep afterwards. It compared the names of
  `/private/tmp/QuillBackendLock*` and the top level of `/private/tmp`
  before and after. It did the same for the user's own `$TMPDIR`.

On this station the card's `ls /tmp | grep -c '^QuillBackendLock'` would
count 0 whatever `/tmp` holds. The shell profile aliases `ls` to a lister that
puts an icon before each name. The counts here come from `find` and Python.

### Results at the candidate

All 12 rows are registered on macOS.

| Row | Direct run | `TMPDIR` afterwards | New `QuillBackendLock*` |
| --- | --- | --- | --- |
| `rfc0017_public_preview_decision_contract` | pass, 1.43 s, 44 tests | empty | none |
| `application_mcp_context_helper` | pass, 0.54 s | empty | none |
| `application_agent_crash_recovery` | pass, 46.16 s | empty | none |
| `application_agent_materialization_crash_recovery` | pass, 40.58 s | empty | none |
| `application_attended_launch_contract` | pass, 20.87 s | `bpr-activate-5335cb89d0717ebb.sock` | none |
| `runtime_process_surface` | pass, 0.61 s | `bpr-activate-9a5a06119ba39cd8.sock` | none |
| `application_device_binding_relaunch` | pass, 9.08 s | empty | none |
| `application_profile_activation_public_path` | pass, 4.85 s | `act-s_eaeqeu-agent-workspaces-v1/`, mode `0700` | none |
| `application_profile_owner_admission_public_path` | pass, 2.45 s | empty | none |
| `quill_logging` | pass, 0.38 s | empty | none |
| `quill_logging_backpressure` | pass, 0.09 s | empty | none |
| `application_logging` | pass, 6.75 s | empty | none |

- **None of the five kinds appeared.** No `bpr-public-path-*`,
  `bpr-mcp-canary-*`, `bpr` followed by 8 characters, or `bpr-tidy-db-*` was
  left in any `TMPDIR`. No row added an entry to `/private/tmp`. No
  `QuillBackendLock*` was created. `/private/tmp` held the same 69 lock files,
  dated 2026-09-21 and 2026-09-23, before and after these runs.
  Nothing new of this project's appeared in the user's `$TMPDIR` either. Its
  only new files were `att-*-postings-*.bin`, which a desktop indexer that
  another process runs held open.
- **Through ctest, as registered**, the same 12 rows passed 12 of 12.
  `quill_logging` and the logging rows are the logging-adapter rows that the
  change must pass on macOS.
- **The ctest runs for [[BPR-1050]]** (09:05:25Z to 09:23:52Z) included both
  agent crash-recovery courses and added no lock file either. The lock's path
  ignores `TMPDIR`, so a ctest run can show this one kind on macOS.

### Three entries outside the card's kinds

The card's expectation was that `find` prints nothing. It printed three
entries. None is one of the five kinds, and each sits in the directory that
the runner owns and deletes when the row ends:

- **`bpr-activate-<digest>.sock`**, from `application_attended_launch_contract`
  and `runtime_process_surface`. This is the profile activation endpoint.
  `profile_activation.cpp`'s `runtime_directory()` puts it in `$TMPDIR` on
  macOS and in `$XDG_RUNTIME_DIR` on Linux. Both courses end an owner with a
  signal, so the owner never closes its endpoint. The Linux measurement gave
  each run a fresh `XDG_RUNTIME_DIR` and removed it afterwards, so it could
  not have seen the same thing.
- **`act-<random>-agent-workspaces-v1/`**, from
  `application_profile_activation_public_path`. On macOS
  `private_fixture_parent()` roots the course's `act-*` directory in
  `BPR_TEST_SOCKET_ROOT`, which is the runner's directory. The course removes
  `act-*`, but not the workspace directory that the application makes beside
  it. On Linux the course makes its own parent under `/tmp` and removes the
  whole parent, so nothing is left.

Under ctest the runner removes its directory, and the three entries go with
it. The 51 row runs that ctest made on this station today left no `bpr-net-*`
directory in `/private/tmp`. It holds the same eight as before these runs,
all dated 2026-09-20 to 2026-09-23. They are not attributed here and were
left alone. The user's own `$TMPDIR` does hold 20 `bpr-activate-*.sock` and
one `bpr-public-path-*` from 2026-09-19 to 2026-09-22. They were made by
earlier work on this station outside the runner, and none is from these runs.
So a killed owner does leave its endpoint behind on macOS whenever the process
runs outside the runner. That has been reported to the coordinator. This card
does not take it.

### Does the sandbox let the process create the lock? Yes

The negative control ran in two steps.

1. `options.check_backend_singleton_instance = false;` was deleted from
   `src/adapters/logging/src/quill_log.cpp`, and nothing else changed. Then
   `app_better_pull_review` and `quill_logging` were rebuilt.
2. The same driver ran two rows:
   - `application_attended_launch_contract` **passed** in 20.99 s, and it left
     **`/private/tmp/QuillBackendLock30415`**: empty, mode `0644`, created at
     09:28:24Z, and held by no process. The course ran as pid 30408, and the
     next row started as pid 30428. Only this project's binaries link Quill.
     So 30415 was an application the course launched and ended. Under the
     derived profile, the application created the file in `/tmp`, and Quill
     did not skip the check.
   - `quill_logging` **failed** with exit 1:
     `a running backend creates no Quill lock file in /tmp`. As on Linux, the
     failing `expect` left its `qt_temp-*` in its `TMPDIR`.

**Restored.** The file was copied back from the saved original. Its SHA-256,
`c7c588af08ffb775036de7ee201955d3cfce4756aaac108a46ee6ff379263677`, is the
same as at `09f3d48f`, and `git diff` is empty. The whole tree was rebuilt:
33 steps, exit 0, no `warning:` line. Both rows were run directly again. The
course passed with no lock file, and `quill_logging` passed.
`/private/tmp/QuillBackendLock30415` was removed by its exact name. It is the
only entry this station removed from `/private/tmp`, apart from its own 16
`bpr1051-direct-*` roots, which were each removed by the name the driver
recorded.

### Acceptance state after this run

- **AC4: pass on macOS.** Over the 12 rows, which are all registered here,
  a direct run under the row's own sandbox profile, with a kept `TMPDIR`,
  passed every row. It left none of the five kinds, and it created no Quill
  lock. The sandbox allows the lock. The only thing that stops it is the
  product's setting, and the negative control brings the lock back.
- **AC5: still owed by bpr-main-win.** The card stays In Progress.

## Windows, AC5: ten of eleven rows measured (bpr-main-win, 2026-09-24)

Station `bpr-main-win`: Windows 11 (10.0.26200), MSVC 19.51, Qt 6.11.2,
`build/windows-dev` (Visual Studio 18 2026 generator, Debug). The head was
`bcd57840`, which contains `09f3d48f`. It was reconfigured and rebuilt, with
exit 0 and no warning or error line. The card's loop ran as a `.ps1` file,
under a `.cmd` file that loads the compiler environment. Each row had `TEMP`
and `TMP` pointed at a new, empty directory under the user's `%TEMP%`. After
the row, the directory was listed recursively and then removed. The logs and
per-row listings are station records under
`build/logs/bpr-1051-main-win/` and are not committed.

### Registration on Windows

All 11 rows in the card's Windows list are registered here. The card's list
leaves out `quill_logging_backpressure`, which is `UNIX`-only. Ten rows are
`WINDOWS_UNCONTAINED`, so they use the `TEMP` they are given.
`application_profile_owner_admission_public_path` is contained. Its launcher
gives it the container's `AC\Temp`, so this measurement cannot see what it
leaves there. That directory
(`%LOCALAPPDATA%\Packages\bprautomatedtest\AC\Temp`) was empty after the row.

### Results

| Row | Result | Left in its directory |
| --- | --- | --- |
| `rfc0017_public_preview_decision_contract` | Passed, 4.40 s, 44 tests `OK`, in a short directory (below) | nothing |
| `application_mcp_context_helper` | Passed, 0.28 s (`mcp-context-helper-tests:pass`) | nothing |
| `application_agent_crash_recovery` | Passed, 52.99 s | nothing |
| `application_agent_materialization_crash_recovery` | Passed, 43.99 s | nothing |
| `application_attended_launch_contract` | Passed, 20.52 s (`ATTENDED_LAUNCH_OK`) | nothing |
| `runtime_process_surface` | Passed, 2.00 s (`RUNTIME_SURFACE_OK`) | nothing |
| `application_device_binding_relaunch` | Passed, 15.67 s (`DEVICE_BINDING_RELAUNCH_OK`) | nothing |
| `application_profile_activation_public_path` | **Not run.** Held back until the desktop's foreground is cleared (below) | not measured |
| `application_profile_owner_admission_public_path` | Failed, 0.21 s, contained: `SEED_REGISTRY_REFUSAL:0`, `FAILED: seed registry`. This is [[BPR-0907]]'s known contained failure | nothing, and the container's `AC\Temp` was empty too |
| `quill_logging` | Passed, 0.96 s (`quill-logging-tests:pass`) | nothing |
| `application_logging` | Passed, 7.23 s | nothing |

- **The logging-adapter change passes on Windows.** `quill_logging` and
  `application_logging` pass. On Windows the Quill lock is a named mutex, so
  there is no file to look for.
- **The measurement's own directory name broke one row.** With the card's
  name, `bpr1051-<guid>`, `rfc0017_public_preview_decision_contract` failed
  13 of 44 tests. The first failure was
  `scratch git ['add', '-A'] failed: fatal: unable to stat '…/01-fixture-graphql-refusal.json': Filename too long`,
  and the rest followed from it
  (`RFC0017_PUBLIC_PREVIEW_DECISION_REFUSED:head commit could not be read from git`).
  The reason is the path length:
  - The name adds 45 characters to `%TEMP%`, which puts the scratch
    repository's deepest file at 278 characters. That is past `MAX_PATH`.
  - The row's scratch Git runs with `GIT_CONFIG_GLOBAL` and
    `GIT_CONFIG_SYSTEM` set to the null device, so the station's
    `core.longpaths` does not apply there.
  - As registered, with the ordinary `%TEMP%`, the same file is 233
    characters, and the row passes 44 of 44 in 4.39 s.
  - In a short new directory, `%TEMP%\b10513f9` (242 characters), it passes
    44 of 44 and leaves nothing. That is the row's AC5 measurement.
  - The failing run also left nothing, because the module's `tearDownModule`
    removed the template. So on this station the row has about 26 characters
    of headroom in `%TEMP%`'s length. That limit is reported to the
    coordinator.
- **The activation course is held back.** This station had no clearance to
  use the desktop's foreground while another station's GUI courses were
  running on it. The course runs offscreen, but its handoff calls
  `AllowSetForegroundWindow`
  (`src/adapters/sqlite/src/profile_activation.cpp`). It will be measured
  with the same loop when the desktop is cleared.

### A Windows-only kind outside the card's five

[[BPR-1050]]'s Windows sweep ran 154 of the 156 `application_` rows with the
ordinary `%TEMP%`, and the top level of `%TEMP%` was listed before and after.
The sweep added none of the card's five kinds. It added three entries of
another kind. Each was then attributed by running its row alone with a new,
empty `TEMP`:

- `application_attended_recent_replay` leaves two `attended_recent_replay-XXXXXX`
  directories on every run. It makes four temporary roots, and it starts a
  desktop application in two of them.
- `application_personal_discovery_public_path` leaves one
  `personal_discovery_public_path-XXXXXX` on every run.

Each is the course's own `QTemporaryDir`. Its `data/` holds
`profile-runtime.lock` and the `state/v1` SQLite stores. Some stores still
have their `-wal` and `-shm` files. The directory grants only the owner, with
full control, so its removal was not refused for permission. This suggests that files were
still open when the directory was due to be removed. Windows cannot delete an
open file, and POSIX can. That would explain why the Linux per-row
measurement saw nothing from these rows. The cause was not traced here.

These rows are not among AC1's, and the fix belongs in the two courses'
teardown, so it is not taken here. It is reported to the coordinator for its
own card. `%TEMP%` on this station still holds 21 such directories from
2026-09-23, made by earlier runs, and they are left to the operator. This
station removed the six its own runs made today, each by its exact name. Two
batches of three were made at 02:28 to 02:30 and 03:24 to 03:25, local time,
inside its runs of these rows.

### Acceptance state

- **AC5: open, for one row.** Ten of the eleven Windows rows were measured.
  None of them leaves anything in its temporary directory. The owner
  admission row fails in [[BPR-0907]]'s known way, and it is contained. The
  activation course waits for foreground clearance.
- AC1 to AC3 and AC6 (Linux) and AC4 (macOS) are unchanged, and each passes.
  The card stays In Progress until the activation course is measured on
  Windows.

### The activation course, measured (bpr-main-win, 2026-09-24)

The coordinator cleared the desktop's foreground. Before the run, and again
after, the foreground window was a terminal, and no WSLg (`msrdc`) window was
open. The same loop ran with the card's directory name, at head `93b3c53e`,
with the build from `bcd57840`. Only `docs/` changed in between.

| Row | Result | Left in its directory |
| --- | --- | --- |
| `application_profile_activation_public_path` | Passed, 13.44 s (`PROFILE_ACTIVATION_OK`), uncontained as registered, 0 refusal lines | nothing |

**AC5: pass on Windows.** The eleven rows in the card's list are all
registered here. Ten ran uncontained and left nothing in their temporary
directory. The one contained row, owner admission, fails in [[BPR-0907]]'s
known way, and it left nothing in its container's temporary directory. The
logging-adapter rows pass. There is no Quill lock file on Windows.

## Finished (bpr-main-win, 2026-09-24)

| Criterion | Station | Candidate | Observation |
| --- | --- | --- | --- |
| AC1 | Linux | `ff9bc9e2` | Each of the five kinds is attributed to its rows, from a per-row measurement over all 423 rows |
| AC2 | Linux | `09f3d48f` | The named rows pass, each `TMPDIR` is empty afterwards, and `/tmp` gains no lock file. The whole suite with `TMPDIR` unset adds nothing of this user's to `/tmp` |
| AC3 | Linux | `09f3d48f` | Reverting each fix brings its kind back, and `quill_logging` fails its new assertion |
| AC4 | macOS | `bcd57840` | 12 of 12 rows pass in direct sandboxed runs with a kept `TMPDIR`, leaving none of the five kinds and no lock. The sandbox allows the lock, and the negative control brings it back |
| AC5 | Windows | `bcd57840` | The eleven rows, as above |
| AC6 | Linux | `09f3d48f` | The canary removes only the directory it created |

Recorded here but not taken by this card, and each reported to the
coordinator:

- the two Windows courses that leave their own `QTemporaryDir` roots;
- the macOS activation endpoints that a killed owner leaves outside the
  runner;
- `rfc0017`'s `MAX_PATH` headroom.
