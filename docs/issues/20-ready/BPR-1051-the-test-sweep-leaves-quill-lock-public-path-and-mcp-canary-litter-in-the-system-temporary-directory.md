---
schema: "implement-rfc.card.v1"
id: "BPR-1051"
title: "The test sweep leaves QuillBackendLock*, bpr-public-path-* and bpr-mcp-canary-* litter in the system temporary directory"
status: "Ready"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, the station's shared /tmp during the 2026-09-23 runs","counted_at":"d4f9d07d","related_cards":["BPR-1047","BPR-1044"],"reservation":"docs/issues/_reservations/BPR-1050--BPR-1051--e733678668d2.json"}
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
