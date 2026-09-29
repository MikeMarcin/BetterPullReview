---
schema: "implement-rfc.card.v1"
id: "BPR-1128"
title: "Thirteen test scratch roots named from the clock or process id reuse whatever is already at that name, and an agent-canary marker is removed before it is made"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux", "mac", "win"]
metadata: {"found_by":"BPR-1126's AC4 scan, its 'Weaker, not changed here' item","reservation":"docs/issues/_reservations/BPR-1127--BPR-1128--e21108c796a5.json","related_cards":["BPR-1126","BPR-1127","BPR-1121"]}
---

# BPR-1128: Thirteen test scratch roots named from the clock or process id reuse whatever is already at that name, and an agent-canary marker is removed before it is made

## Finding

[[BPR-1126]]'s scan found these and gave them this title. Each root is named
in the system temporary directory from `steady_clock` nanoseconds, or from an
object address and the process id. None of them checks that the name was
free: `create_directory` and `create_directories` return `false`, and throw
nothing, when a directory, or a link to one, is already at the name. So
what is there is reused. The course works in it, and the root's owner
removes it, with everything in it, at the end. A link at the name goes
further. The Git contracts narrow it with `std::filesystem::permissions`,
which follows a link, and resolve it with `std::filesystem::canonical`, so
the root becomes the link's target. The stores' own rule,
`make_storage_root_owner_private`, narrows a link's target before it
refuses the link.

To plant, someone has to know the exact name first. No fixed or counter
name is removed, which is why [[BPR-1126]] ranked these below its own four.
Rechecked at `b8944a06`:

| # | Linux rows | Source | Name | Made | Then |
| --- | --- | --- | --- | --- | --- |
| 1 | `sqlite_review_mutation_recovery_storage` | `tests/sqlite_contract/review_mutation_recovery_storage_tests.cpp` | `bpr-mutation-recovery-<address>-<pid>`, lines 36 to 41 | `create_directory`, line 42 | narrowed by `chmod` (line 50) or, on Windows, the suite's DACL (line 48); removed at line 57 |
| 2 | `sqlite_profile_storage_root` | `tests/sqlite_contract/profile_storage_root_tests.cpp` | `bpr-0694-roots-<clock>`, line 29 | line 30 | narrowed line 33, resolved line 34, removed line 40 |
| 3 | `sqlite_profile_registry_storage` | `tests/sqlite_contract/profile_registry_storage_tests.cpp` | `bpr-0691-registry-<clock>`, line 30 | line 31 | lines 34, 35 and 41 |
| 4 | `sqlite_window_review_restoration_storage` | `tests/sqlite_contract/window_review_restoration_storage_tests.cpp` | `bpr-0709-restoration-<clock>`, line 28 | line 29 | lines 32, 33 and 39 |
| 5 | `sqlite_private_store_open` | `tests/sqlite_contract/private_store_open_tests.cpp` | `<name>-<clock>`, lines 71 and 72 | line 73 | narrowed line 76, removed line 82 |
| 6 | `sqlite_device_binding_storage` | `tests/sqlite_contract/device_binding_storage_tests.cpp` | `bpr-device-binding-<clock>`, line 541 | line 542 | narrowed line 545. The course removes the root itself at line 561, and its guard removes whatever is at the name again at exit |
| 7 | `contained_catalog_probe` | `tests/sqlite_contract/contained_catalog_probe_tests.cpp` | `bpr-contained-catalog-probe-<clock>`, lines 33 to 36 | `create_directories`, line 37 | not narrowed; removed by the guard at line 39 |
| 8 | `git_anonymous_public_read` | `tests/git_contract/anonymous_public_read_tests.cpp` | `bpr-git-anon-<clock>`, lines 59 to 62 | `create_directories`, line 63 | `permissions` lines 66 to 69, resolved line 70, removed line 80 |
| 9 | `git_cache_contract` | `tests/git_contract/git_cache_contract_tests.cpp` | `bpr-git-cache-<clock>`, lines 335 to 340 | line 341 | lines 344 to 347, 348 and 358 |
| 10 | `git_process_contract` | `tests/git_contract/git_process_contract_tests.cpp` | `bpr-<label>-<clock>`, lines 326 to 331 | line 332 | lines 335 to 338, 339 and 349 |
| 11 | `git_haves_trimmed_pack_update`, `_missing-object`, `_unconnected` | `tests/git_contract/haves_trimmed_pack_integration_tests.cpp` | `bpr-haves-<label>-<clock>`, lines 194 to 196 | its children, lines 197 to 199 | lines 202 to 206, 208 and 214 |
| 12 | `epoch_maintenance_integration` | `tests/git_contract/epoch_maintenance_integration_tests.cpp` | `bpr-maintenance-<label>-<clock>`, line 567 | its children, lines 568 and 569 | lines 573 to 584, 586 and 594. The two kill-recovery rows pass a root their driver made, which this card does not change |
| 13 | none: Windows only | `tests/adapters_qt/agent_launch_canary_tests.cpp` | `bpr-agent-breakaway-<clock>`, lines 925 and 926 | `create_directory`, line 927 | guard line 929, removed line 962 |

**The canary's marker.** `test_timeout_terminates_the_descendant_process_tree`
names a marker file `bpr-agent-descendant-<clock>` in the temporary directory
(`agent_launch_canary_tests.cpp`, lines 679 to 682). It removes whatever is
at that name (line 684), and then a descendant of the canary appends its
heartbeat to the name with `std::ofstream` (`tests/fixtures/agent/agent_canary_main.cpp`,
`heartbeat`, lines 379 to 387), which follows a link. A link put back at the
name between the removal and the first heartbeat is written through.
The marker is removed at line 706 and by its guard (line 687). The
Windows-only course
`test_windows_assignment_failure_has_no_pre_assignment_side_effect` does the
same with `bpr-agent-pre-assignment-<clock>` (lines 1044 to 1049).

## Scope

Revised 2026-09-24, before the change was committed, on the coordinator's
direction. The Windows station is changing the Windows branch of
`tests/sqlite_contract/scratch_root.hpp`: [[BPR-1126]]'s two rows inside the
AppContainer are refused access to their own roots. So that helper is neither
moved nor edited here. No new helper copies its owner-only access list on
Windows. The scope first said that helper's shared part would move to
`tests/support/` and make every root.

- On POSIX, every root in rows 1 to 12, and the directory holding the
  canary's marker, is made by `mkdtemp`:
  - under a random name, and only if nothing is at that name;
  - mode `0700` from creation;
  - owned from creation by [[BPR-1121]]'s `scratch_guard`;
  - removed only by the test that made it.
- The four roots narrowed by the stores' own rule under a plain label, rows
  2, 3, 4 and 6, use the SQLite contracts' helper, unchanged, on every
  platform. The rest use a new POSIX-only helper,
  `bpr::tests::make_scratch_directory` in `tests/support/scratch_directory.hpp`.
- The canary's descendant writes its marker into such a directory, instead of
  at a name the test first removed.
- On Windows, rows 1, 5 and 7 to 12, and the marker, still make their roots
  as `HEAD` did. Each keeps `HEAD`'s code in an `#if defined(_WIN32)` branch
  marked `[[BPR-1128]]`. Row 13 and the Windows-only marker are built only on
  Windows, and are not edited blind. The follow-up is to unify the two
  helpers after the Windows station's fix lands, with a Windows branch that
  gives each row the access it needs, and to move these Windows roots onto
  it. Its title is given below.
- `tests/AGENTS.md` names both helpers.

## Acceptance

- AC1: On POSIX, each root in rows 1 to 12, and the directory holding the
  canary's marker, is made by `mkdtemp` through one of the two helpers:
  under a random name, created only if nothing is at the name, mode `0700`
  before anything is written in it, and owned from creation. None of these
  sources reuses what is at a name, or removes anything before it makes a
  root. Revised 2026-09-24, before the change was committed, with the scope.
  It said one shared helper, on every platform.
- AC2: Negative control. At `HEAD`, a trial hook pins each source's name to a
  value the trial knows. The hook is never committed. A sentinel directory
  planted at the first pinned name is reused by the row, and removed at its
  end. The card records what `HEAD` does with a link planted there that
  points to a sentinel directory the trial owns. For the marker, a planted
  sentinel file is removed. A link put back at the name while the course runs
  is written through by the descendant. With the candidate, every plant, and
  the sentinel directory's contents, are left exactly as they were.
- AC3: The Linux rows pass with the candidate and print the same lines as at
  `HEAD`, apart from random names and timings. One line changes on purpose.
  `contained_catalog_probe` prints the catalog's answer. For a root with the
  umask's mode it was a refusal, `2` (`invalid_permissions`). For an
  owner-private root it is `constructed`. A passing run leaves no root.
  Forced failures, made with [[BPR-1121]]'s trial mechanism, leave none
  either. With unwinding turned off, the same failures do leave roots: the
  negative control that shows the trial can see a leak. Revised 2026-09-24,
  before the change was committed. The probe's line was found to change, for
  the reason given.
- AC4: The change is portable. The sources build, and their rows pass, on
  macOS and Windows. On Windows, rows 2, 3, 4 and 6 now use the SQLite
  helper's Windows branch. [[BPR-1126]]'s uncontained rows pass with it, and
  these four are uncontained. The other changed sources compile `HEAD`'s code
  there, and the new helper declares nothing there. Revised 2026-09-24,
  before the change was committed, with the scope. It said that row 13 and
  the Windows-only marker would be changed on the Windows station under this
  card. They move to the follow-up.
- AC5: Gates: `/usr/bin/clang-format` 22, `git diff --check`,
  `architecture_boundaries`, `msvc_secure_crt_contract`, `DOCUMENTATION_OK`,
  and no scoped `tidy.py` finding on a changed line.

## Boundaries

- Test sources and test support only. No product source, no row
  registration and no composition pin.
- `tests/sqlite_contract/scratch_root.hpp` is not edited or moved. Its
  Windows branch is the Windows station's change.
- The two macOS-only roots are BPR-1127's, done on the Mac with its own
  `mkdtemp` and `scratch_guard`:
  `tests/platform/macos_capsule_identity_tests.cpp` and
  `tests/platform/macos_supervisor_identity_continuity_tests.cpp`. They are
  not touched here.
- Every run goes through `ctest`, serially and offscreen. `DISPLAY`,
  `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` are unset, and `HOME` and
  the XDG roots are fresh absolute paths. Nothing is created in
  `/run/user/1000`.
- Trial plants are made only at paths the trial checked were absent. They
  hold only sentinel data the trial wrote. A link's target is a sentinel the
  trial made. Every plant is removed by its exact name afterwards.

## Fixed on POSIX at `b16bf039`, verified on Linux — bpr-main-linux, 2026-09-24

Claimed at `5a405fd2`. The change is one commit, `b16bf039`. It touches
test sources, one new test-support header and `tests/AGENTS.md`. No product
source, row registration or composition pin changed, and
`tests/sqlite_contract/scratch_root.hpp` is unchanged.

It was verified as `437726cd`, on `0453103d`. `b16bf039` is that commit
rebased over twelve commits, which change none of its 15 files and no
source its 18 rows build: only `src/adapters/qt`, its tests, scripts and
cards.

The coordinator also asked for a fix to [[BPR-1121]]'s shared header, for
that card's AC7. It is a separate commit, `053abefe`, recorded on that card.

### The change

- **`tests/support/scratch_directory.hpp`**, new, POSIX only.
  `bpr::tests::make_scratch_directory(stem)` makes `<stem>-XXXXXX` in the
  temporary directory with `mkdtemp`: at a name nothing had, mode `0700`.
  It holds a `scratch_guard` until the directory is resolved, then returns
  the resolved path. The caller owns it from then. On Windows the header
  declares nothing.
- **Rows 2, 3, 4 and 6** use [[BPR-1126]]'s `sqlite_contract::scratch_root`,
  unchanged, with the labels `0694-roots`, `0691-registry`,
  `0709-restoration` and `device-binding`. Their own constructors, clock
  names, removals and narrowing calls are gone. The helper narrows by the
  stores' rule on Windows, and `mkdtemp` makes `0700` on POSIX.
  - Row 6's course used to remove its whole root midway. Its guard then
    removed whatever was at that name at exit.
  - Its cancelled-load check now uses a name under the root that was never
    made. So nothing but the owner removes the root.
- **Rows 1, 5 and 7 to 12** call the POSIX helper, in place of the lines
  that named and made the root. On Windows each keeps `HEAD`'s statements,
  in an `#if defined(_WIN32)` branch marked `[[BPR-1128]]`.
  - Row 1 keeps its `chmod` and its DACL. Row 5 keeps its names and the
    stores' rule.
  - On POSIX, rows 11 and 12 now take their guard before they make the
    root's children. Row 12 still adopts a root its driver made, as before.
- **The canary's marker** is `heartbeat`, in a `bpr-agent-descendant-XXXXXX`
  directory the course made and guards. On Windows it is `HEAD`'s marker.
- **`tests/AGENTS.md`** names both helpers, and forbids a clock, counter or
  process-id name that uses what is already there. It says a child's file
  goes inside such a root, and that a Windows root must carry the access its
  row needs.

A first candidate, not committed, moved the SQLite helper's generic part to
`tests/support/`. It was set aside on the coordinator's direction (see
Scope). Its trials are kept with the station records and are not evidence
for this commit.

### Verification on Linux

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2. `build/dev` is Debug, with warnings
as errors.

- The whole graph was rebuilt with the candidate, with no `warning:` line.
- `build/core-only` compiled the sources of rows 1 to 7, with none either.
- Every run went through `ctest`, serially and offscreen. `DISPLAY`,
  `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` were unset. `TMPDIR` was
  unset unless a trial below sets it. `HOME` and the XDG roots were fresh
  absolute paths.
- No run added a `bpr-` name to `/run/user/1000`.

**AC2, the negative control at `HEAD`.** These are `b8944a06`'s sources,
built at `5a405fd2`. A trial hook, not committed, sat only in the lines that
compute each name. When `BPR_TRIAL_NAME_SEED` is set, it returns a value the
trial chose in place of the clock (rows 2 to 12, and the marker), and in
place of the address and process id (row 1). With it, each row made its
roots at names the trial knew, such as `bpr-git-cache-4242420000` and
`bpr-mutation-recovery-4242420000-77777`. A plant went at each row's first
name in `/tmp`, after the trial saw that name was absent. Afterwards the
sources were restored to their recorded SHA-256, and `git diff` was empty.
The executables were rebuilt, and `strings` finds no `BPR_TRIAL` in them.

| Plant at the first name | `HEAD` | Candidate |
| --- | --- | --- |
| A sentinel directory holding a sentinel file | All 14 rows use it: the course's own files appear in it. All 14 remove it at the end, sentinel file and all, and pass | All 14 left, with the same inode and bytes |
| A link to a sentinel directory, mode `0755` | The seven Git rows (8 to 12) follow it. They narrow the target to `0700`, work in it and remove it, sentinel file and all, and pass. Rows 1 to 6 narrow the target to `0700` through the link, then refuse it and fail, and the link is removed. Row 7 refuses it (`CONTAINED_CATALOG_PROBE_OK:1`) and removes the link | All 14 links left, with the same inode and target. Each target is left at `0755`, with its file |
| At the canary's marker: a sentinel file | Removed before the descendant writes. The course then writes its own file there and removes it | Left |
| At the marker: a sentinel directory | The course fails (`cannot get file size: Is a directory`), and its guard removes the directory, sentinel file and all | Left |
| At the marker: a link to a sentinel file, put back by a thread whenever it is missing | Followed on the first run. The descendant appended 74 bytes of heartbeat to the sentinel file, and the row passes | The link is never removed. The thread put it back 0 times in 5 runs, and the file is unchanged |

**AC1, the roots as made.** A trial polled a private, empty `TMPDIR` while
each row ran.

- Every root the poll saw was a real directory that this user owns, mode
  `0700`, at a random name such as `bpr-git-cache-C7GgB5` or
  `bpr-0883-日本語-😀-HMjUgv`.
- It saw 6 roots for row 1, 30 for row 3, 41 for row 4, 2 for row 5, 1 for
  rows 6 and 7, 4 for row 8, 3 for row 9, 26 for row 10, 1 for each of
  row 11's three rows, and 4 for row 12. It also saw the canary's marker
  directory.
- The poll is too slow for row 2's roots, which live for well under a
  millisecond. Its forced failures below show them.
- The directory was empty after every run.
- The canary's working directory, made at line 364 and not in this card's
  class, keeps the umask's mode.

**AC3, the rows.**

| Check | Result |
| --- | --- |
| Integration: all 56 Linux rows of [[BPR-1121]]'s class, which include this card's 18, after the whole graph was rebuilt | 56 of 56 pass. Compared with the same rows at `053abefe`, before this change. The other commits since then change only the Qt diff workbench. 52 print the same bytes; `application_profile_management_public_path`, `qt_agent_launch_canary` and `qt_markdown_body_1_0` differ only in a random temporary name or a millisecond timing; and the probe prints `constructed` where it printed `2` (AC3). No row added a top-level `/tmp` entry |
| A passing run's top-level `/tmp` entries | none added |
| Forced failures with [[BPR-1121]]'s trial hook in the shared `expect`, not committed, each run with its own private `TMPDIR`. N was a third and two thirds of each row's count, or 1 for the probe, whose count is 2. The canary also failed at `the descendant wrote its heartbeat`, inside the marker's course | 30 of 30 fail with their `FAILED:` line and leave nothing |
| The same failures with unwinding turned off (`BPR_TRIAL_NO_UNWIND`), the negative control | 28 of 30 leave the root that existed. The other two are the canary's failures at N = 99 and 199, where no root exists: neither variant leaves one |

The hooked header was restored to its recorded SHA-256, and the graph was
rebuilt. `strings` finds no `BPR_TRIAL` in any of the 46 rebuilt executables.

**AC5, the gates.**

| Gate | Result |
| --- | --- |
| `/usr/bin/clang-format` 22 `--dry-run -Werror` on the header and the 13 sources; `git diff --check` | pass |
| `architecture_boundaries`, `coding_format_check`, `msvc_secure_crt_contract` | pass |
| `_coding-standard/scripts/tidy.py` on the 13 sources, which also covers the new header | three findings, all on lines this change does not touch: `epoch_catalog_legacy_fixture.hpp:1` (`pragma once`), `git_cache_contract_tests.cpp:1351` and `haves_trimmed_pack_integration_tests.cpp:100`. None in the new header |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

The logs are station records and are not committed. They are in
`build/logs/bpr-1128/`:

- `candidate2.patch`, the change as tested, on `candidate2.base`, and
  `candidate2-committed.patch`, the commit, which matches it;
- `build-*.log`, `gates.log`, `tidy-candidate2.log`,
  `integration-candidate2.log` and its comparison;
- `trial/`, holding the hooks and their appliers (`pin_names.py`,
  `bpr_name_pin.hpp`, `apply_hooks.py`, `bpr_trial_hook.hpp`), the drivers
  `trial.py` and `forced.py`, and the per-run logs and `results.json` files
  in `head-baseline/`, `head-pinned/`, `candidate2/` and `forced2/`.

### Evidence owed elsewhere (AC4)

On the macOS station:

```sh
cmake --build --preset dev
ctest --preset dev -j1 --output-on-failure -R '^(sqlite_(review_mutation_recovery_storage|profile_storage_root|profile_registry_storage|window_review_restoration_storage|private_store_open|device_binding_storage)|contained_catalog_probe|git_(anonymous_public_read|cache_contract|process_contract|haves_trimmed_pack_(update|missing-object|unconnected))|epoch_(maintenance_integration|kill_recovery|transaction_kill_recovery)|qt_agent_launch_canary|agent_parent_death)$'
```

On the Windows station, after its change to the SQLite helper:

```powershell
cmake --build --preset windows-dev
ctest --preset windows-dev -j1 --output-on-failure -R "^(sqlite_(review_mutation_recovery_storage|profile_storage_root|profile_registry_storage|window_review_restoration_storage|private_store_open|device_binding_storage)|contained_catalog_probe|git_(anonymous_public_read|cache_contract|process_contract|haves_trimmed_pack_(update|missing-object|unconnected))|epoch_(maintenance_integration|kill_recovery|transaction_kill_recovery)|qt_agent_launch_canary|agent_parent_death|msvc_secure_crt_contract)$"
```

On each station, every row should pass. Afterwards the temporary directory
should hold no entry starting `bpr-mutation-recovery-`, `bpr-0694-roots-`,
`bpr-0691-registry-`, `bpr-0709-restoration-`, `bpr-0883-`,
`bpr-device-binding-`, `bpr-contained-catalog-probe-`, `bpr-git-`,
`bpr-haves-`, `bpr-maintenance-` or `bpr-agent-descendant-`. On Windows that
includes the AppContainer's own temporary directory, for the probe and the
canary, which run inside it. The card stays In Progress on this station's
claim until those runs are supplied.

### Left for the follow-up (title given)

Once the Windows station's change to `tests/sqlite_contract/scratch_root.hpp`
has landed, unify it with `tests/support/scratch_directory.hpp` into one
helper with a Windows branch. That branch makes a directory at a random name
only if nothing is there, and gives it the access its row needs. A row
inside the AppContainer needs the container's SID. Neither
`QTemporaryDir`'s access list nor an owner-only list gives it that
([[BPR-1126]]'s Windows record). Then move onto it:

- the Windows branch of each source marked `[[BPR-1128]]. Windows keeps its
  root as it was` (`grep -rn 'BPR-1128\]\]. Windows keeps' tests`). These
  are rows 1, 5 and 7 to 12, and the canary's marker. The probe and the
  canary run inside the AppContainer;
- row 13, `bpr-agent-breakaway-<clock>`, in
  `test_breakaway_descendant_dies_with_supervised_job`. It is made with
  `create_directory` and removed at the course's end;
- the Windows-only marker `bpr-agent-pre-assignment-<clock>`, in
  `test_windows_assignment_failure_has_no_pre_assignment_side_effect`. It is
  removed before the child could write it.

### Seen, not changed (title given)

The canary fixture has a marker of its own.
`tests/fixtures/agent/agent_canary_main.cpp`, lines 711 to 727, names a
default heartbeat marker `bpr-agent-descendant-<pid>` in the temporary
directory when it runs as `…-descendant-wait` without `--spawn-descendant`.
Its descendant appends to that name with `std::ofstream` (`heartbeat`, lines
379 to 387), which follows a link. `application_agent_invocation_cancellation_contract`
runs it that way
(`tests/end_to_end/agent_invocation_composition_public_path.cpp`, line 634),
reads the name back (lines 1396 to 1398), and removes the file at its end
(lines 1459 to 1471). So whatever is at that pid-named name is used and then
removed, and a failing course leaves the file. Nothing removes the name
before the descendant writes, so it is not this card's marker. It needs the
fixture and the end-to-end course to change together.

## macOS, AC4 (bpr-main-mac, 2026-09-25)

The station runs macOS 26.6.2 (25G83) on Apple Silicon, with Apple clang
21.0.0 (`clang-2100.1.1.101`), Qt 6.11.2, and `build/dev` Debug with
warnings as errors. This entry supplies the macOS half of AC4. It does not
move the card, which stays on bpr-main-linux's claim. The Windows run is
still owed.

### The build and the rows

At `06e578de`, which contains `b16bf039` and `11465a0b`, an incremental
`cmake --build --preset dev` exited 0 after 97 steps and printed no
`warning:` line. The rows below then ran the card's macOS command, serially
and verbosely, from 05:21:55Z to 05:36:49Z.

| Rows | Result |
| --- | --- |
| Rows 1 to 7: `sqlite_review_mutation_recovery_storage`, `contained_catalog_probe`, `sqlite_private_store_open`, `sqlite_device_binding_storage`, `sqlite_profile_registry_storage`, `sqlite_profile_storage_root`, `sqlite_window_review_restoration_storage` | 7 of 7 pass, in 0.28 to 2.78 s. The probe prints `CONTAINED_CATALOG_PROBE_OK:constructed:0s`, the line AC3 changed on purpose |
| `qt_agent_launch_canary`, `agent_parent_death` | pass, 85.32 and 28.88 s |
| Rows 8 to 12, and `epoch_kill_recovery` and `epoch_transaction_kill_recovery` | 9 of 9 pass. `git_cache_contract` is the slowest, at 274.34 s |
| In all | **18 of 18 pass**, 894.10 s |
| `/private/tmp` and the user's `$TMPDIR`, listed before and after | No entry named in "Evidence owed elsewhere" appeared. `/private/tmp` gained one `bpr-net-*` root. It belonged to another agent's run of an end-to-end course, which started at 05:34Z and was still running at the listing. `$TMPDIR` gained only a desktop indexer's `att-*-postings` files and the editor's `preamble-*.pch` files |

On macOS a ctest run cannot show a leftover in its own root
([[BPR-1047]]). The runner gives each row `/private/tmp/bpr-net-*/fixture`
as `TMPDIR` and removes it when the row ends. The direct run below keeps the
root, so it can show what a row leaves.

### Direct sandboxed runs: rows 1 and 7 and the canary's marker, with `TMPDIR` through a link

**Why.** Before this card, rows 1 and 7 and the canary's marker named their
roots under `temp_directory_path()` as given. `make_scratch_directory` now
returns `canonical()` of what it made. Under ctest the two are the same,
because the runner resolves the `TMPDIR` it gives each row. On macOS the
user's own temporary directory is `/var/folders/…/T`, and `/var` and `/tmp`
are links into `/private`. There, `canonical()` returns a different path
from `TMPDIR`. No run had covered that case.

**Method.** A scratch driver, not committed, ran each row's command the way
`scripts/run_network_isolated_test.py` does. It used the runner's own
`write_profile()` to write the profile. It gave the child the row's
environment and environment changes, read from `ctest --show-only=json-v1`,
and the runner's four variables. It ran `/usr/bin/sandbox-exec -f <profile>
<command>` from the row's working directory, and judged the row by CTest's
rule. Unlike the runner, it kept its root, `bpr1128-direct-XXXXXXXX/`, and
gave `TMPDIR` as `fixture/` (mode `0700`) through a link. It did that once
under `/tmp` and once under the user's `/var/folders/…/T`.

**Plants.** Before each run, the driver checked that each name below was
absent, then planted at it, in `fixture/`. For each row's stem `S`:

- a sentinel directory, mode `0755`, at `S-PLNTDR` and at `S-4242420000`,
  a clock-style name. Each held a `sentinel.txt` and a `heartbeat` file;
- a link at `S-PLNTLK` to a sentinel directory outside `fixture/`.

For the marker's stem, it also planted a sentinel file at
`bpr-agent-descendant-4242420001`, and a link at `…-4242420002` to a sentinel
file outside `fixture/`. A thread listed `fixture/` every 2 ms while the row
ran. Afterwards, every entry under `fixture/` and under the link targets was
compared with its state before the run: type, inode, mode, and bytes or link
target.

| Row | `TMPDIR` | Result | Made during the run | Plants | Left |
| --- | --- | --- | --- | --- | --- |
| `sqlite_review_mutation_recovery_storage` (row 1) | under `/tmp` and under `/var/folders`, through the link | pass, both | six `bpr-mutation-recovery-XXXXXX` directories in each run, mode `0700`, this user's | all 7 unchanged | nothing |
| `contained_catalog_probe` (row 7) | the same | pass, both, `CONTAINED_CATALOG_PROBE_OK:constructed:0s` | one `bpr-contained-catalog-probe-XXXXXX`, `0700` | all 7 unchanged | nothing |
| `qt_agent_launch_canary`, as built | the same | **fails, both,** at "the child reports the exact request working directory, not the parent cwd", 28 s in, before the marker's course | `bpr-agent-cwd-<clock>`, `0755` | all 9 unchanged | nothing |
| The same, with `TMPDIR` resolved, as the runner gives it | `/private/tmp/…` and `/private/var/…` | pass, both, 69 s | `bpr-agent-cwd-<clock>` (`0755`) and `bpr-agent-descendant-XXXXXX` (`0700`) | all 9 unchanged | nothing |
| The same, built with a trial hook that skips the working-directory course, with `TMPDIR` through the link | under `/tmp` and under `/var/folders` | pass, both, 68 to 72 s | `bpr-agent-descendant-XXXXXX`, `0700` | all 9 unchanged | nothing |

**Why the canary fails through a link, and why that is not this card's.**
`test_real_child_receives_no_parent_credential_or_context_in_argv` makes its
working directory at `temp_directory_path()/bpr-agent-cwd-<clock>` (lines
365 to 371 at `06e578de`). It then compares, lexically, the working
directory the child reports with that path (lines 416 to 421). The child's `getcwd` answers with the resolved
`/private/…` path, so under a `TMPDIR` reached through a link the two
differ. `b16bf039` does not touch that course: in this file it changes only
the marker's course and adds an include. The course is outside this card's
class, as "AC1, the roots as made" already says. Under ctest the row passes,
because `TMPDIR` is resolved there. The canary runs this course before the
marker's, so the direct run needed the hook to reach the marker. The hook
wrapped that one call in a check of `BPR_TRIAL_SKIP_CWD_COURSE`. It was made
only in a scratch `git archive` of `06e578de`, built in a directory of its
own, and never in the shared tree. That scratch tree has been removed.

**So on macOS:** rows 1 and 7 and the marker's course pass with the
canonical path the helper returns, whether `TMPDIR` is resolved or reached
through a link. Every plant beside their names was left exactly as it was,
and nothing was left behind. Each driver root was removed by its exact name
after its comparison. The logs, the driver, the hook's diff and the results
are station records in `build/logs/bpr-1128-mac/`, not committed.

### Seen, not changed (title given)

"The agent canary's working-directory check compares paths lexically, so it
fails when `TMPDIR` is reached through a link." It matters only outside
ctest, such as a direct run with the user's own `TMPDIR` on macOS. It could
compare `canonical()` of both sides. It is left to the coordinator whether
this needs a card.

### Acceptance state after this run

- **AC4 on macOS: met.** The sources build with no warning, and all 18 rows
  pass. The Windows half is still owed, and the card stays In Progress.

## Windows, AC4 (bpr-main-win, 2026-09-29)

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, `build/windows-dev`
Debug with warnings as errors. This entry supplies the Windows half of AC4.
It does not move the card, which stays on bpr-main-linux's claim.

**The build.** At `22d18416`, rows 2, 3, 4 and 6 did not compile on
Windows. Their sources include `scratch_root.hpp`, whose Windows branch
includes `bpr/platform/windows_private_object.hpp`, but their targets did
not link `BetterPullReview::Platform`, as [[BPR-1126]]'s rows do.
`537f718d` adds that link, under `if(WIN32)`, to them and to the other
targets in the same state. It also fixes four MSVC breaks outside this
card. No source of this card changed. With it, the whole graph builds with
no `warning` line.

**The rows**, run with the card's Windows command, serially:

| Rows | Result |
| --- | --- |
| The six `sqlite_` rows, `contained_catalog_probe` | 7 of 7 pass. Rows 2, 3, 4 and 6 run uncontained, through the SQLite helper's Windows branch |
| `git_anonymous_public_read`, `git_cache_contract`, `git_process_contract`, the three `git_haves_trimmed_pack_` rows, `epoch_maintenance_integration`, `agent_parent_death` | 8 of 8 pass |
| `epoch_kill_recovery`, `epoch_transaction_kill_recovery` | **failed** at every boundary: `recovery refs do not contain one exact base/head pair`. The recovered repository held both loose refs, but the course's own `git for-each-ref` listed none. It strips the global config, and with it `core.longpaths`, and the ref's path under the temporary root is longer than `MAX_PATH`. `f9870baa` passes `-c core.longpaths=true` on Windows, as `agent_composition_kill_recovery_tests.py` already did. Then both pass, 36.03 and 57.59 s. Not this card's: the Windows code of these sources is `HEAD`'s |
| `msvc_secure_crt_contract` | fails at `22d18416` for `tour_owner_channel_tests.cpp:170`, outside this card. Passes with `537f718d` |
| `qt_agent_launch_canary` | **fails** contained at `open durable child course`, an RFC-BPR-0023 course that runs before any canary launch. See [[BPR-1139]]'s Windows entry |

The user's temporary directory and the AppContainer's own
(`…\Packages\bprautomatedtest\AC\Temp`) were listed before and after the
run. Neither gained an entry with any of the eleven prefixes. The 140 older
clock-named entries with those prefixes in `%TEMP%` were there before the
run and are unchanged.

**AC4 on Windows: met, except for `qt_agent_launch_canary`.** The sources
build once linked, and 18 of the 19 rows pass, the two epoch rows after a
course fix outside this card. The canary row fails for an RFC-BPR-0023
course, not for this card's change, which compiles `HEAD`'s code on
Windows. The logs are station records in
`C:\Users\mikem\bpr-main-helpers\run\station-0929\` (`1139-1128.log` and
`.xml`, `pass2.log`, `temp-*.txt`), and are not committed.
