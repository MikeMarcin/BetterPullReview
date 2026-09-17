---
schema: "implement-rfc.card.v1"
id: "BPR-0854"
title: "Four source-scan contracts are red on main"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep; confirmed on macOS by dsp-004","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0854: Four source-scan contracts are red on main

## Defect

`bpr-linux`'s full sweep at `a1864fa2` found four source-scan rows failing,
and each fails again when run alone
(`docs/verification/dsp4-linux-confirmation.md`). The integrator reproduced
all four on macOS at `20d99914`: `build/r2-logs/dsp4-linux-sweep-mac-classify.log`,
plus a clean `git archive` export for the two script-only checks. No card
recorded any of them. Integration passes have run focused selections, so the
rows stayed red.

1. **`contained_temporary_usage_contract`**
   (`tests/scripts/contained_temporary_usage_tests.py`,
   `test_no_test_creates_a_temporary_through_tempfile`). Thirteen Python course
   sites call `tempfile` instead of `scripts/contained_temporary_support`:
   - `device_binding_relaunch_contract.py` :63, :92 (`1925d0cc`, 2026-09-17);
   - `profile_activation_contract.py` :28, :63;
   - `profile_owner_admission_contract.py` :33, :49;
   - `profile_window_layout_contract.py` :14 (`ad3abf50`, 2026-09-08);
   - `profile_window_restart_contract.py` :13;
   - `profile_window_review_restoration_contract.py` :59;
   - `rfc0015_cli_publish_fixture_tests.py` :15;
   - `rfc0015_cli_read_fixture_tests.py` :28, :76 (`2625472b`);
   - `tests/github_contract/probe_gh_clone_git_bridge.py` :28 (`cdf6175c`).

   [[BPR-0847]] (:28) and [[BPR-0849]] (:33) each added one site while the
   gate was already red. Their owner-only parents must keep working through
   the contained support. On Linux and macOS, the parent must still be
   owner-only when `BPR_TEST_SOCKET_ROOT` is unset.
2. **`msvc_secure_crt_contract`**
   (`test_every_getenv_read_is_spelled_for_msvc`).
   `tests/sqlite_contract/profile_activation_tests.cpp:39` reads
   `std::getenv("BPR_TEST_SOCKET_ROOT")` without the MSVC spelling
   (`a654669c`, 2026-09-08).
3. **`conditional_branch_identifiers`** (`scripts/verify_conditional_branches.py`,
   `CONDITIONAL_BRANCH_IDENTIFIERS_FAILED:11`). All eleven findings are in
   branches this host does not compile:
   - `src/app/main.cpp:8621`: `schedule_native_notification_run` against the
     declared `scheduleNativeNotificationRun` (`47de7ae8`, 2026-09-08). This
     may be a real misspelling in a branch no platform compiles.
   - `src/app/src/github_application.cpp`:
     - :13375-13378 and :13387-13388: `options` and `config` lambda
       init-captures (`a654669c`);
     - :13899-13905: `moves` and `accepted` init-captures (`a5427499`,
       2026-09-15).

     These look like the gate misreading init-captures. The gate was last
     changed in `26cd2f35`.
4. **`dependency_modernization_contract`**
   (`test_dependency_discovery_is_centralized_and_package_based`).
   `tests/end_to_end/CMakeLists.txt:2199` calls
   `find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)` outside
   central discovery (`2df4d110`, 2026-09-12).

## Scope

- Fix each at its cause.
  - Move the Python course sites to `scripts/contained_temporary_support`
    without weakening their private-parent checks.
  - Spell the `getenv` read the way the MSVC gate requires.
  - Use central Python discovery in the end-to-end CMake.
  - For the conditional-branch findings: fix `main.cpp:8621` if it is a real
    misspelling. Teach the gate lambda init-captures only if the rest are
    false positives, and keep it strict otherwise.
- `src/app/main.cpp` and `github_application.cpp` are pinned by the round-trip
  contract. A change there reseals the pins with a recorded review.

## Acceptance

- All four rows pass on macOS and Linux. `msvc_secure_crt_contract` and
  `contained_temporary_usage_contract` also pass on Windows, if they are
  registered there.
- The courses whose files changed still pass on the platforms where they are
  registered.

## Current frontier (dsp-004, bpr-linux, task-20260917-lnx-sweep-fixes)

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-004` (station
  dispatch `bpr-linux/int-11`), mandate r2. Station `bpr-linux`
  (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`, authority `lane-commit`.
- **Outcome so far.** Items 2 (`msvc_secure_crt_contract`) and 4
  (`dependency_modernization_contract`) are fixed and their rows pass. Item 1
  (the 13 `tempfile` sites) and item 3 (`conditional_branch_identifiers`) are
  recorded below as they land.
- **Changed paths so far.** `tests/sqlite_contract/profile_activation_tests.cpp`
  and `tests/end_to_end/CMakeLists.txt`. No product source and no pinned
  digest changed.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-sweep-fixes/`
  (`0854-*`). None is committed.
- **Still open.** Items 1 and 3, the independent review of the whole diff, and
  the macOS and Windows runs under "For macOS and Windows".

## Item 2: the MSVC spelling

`tests/sqlite_contract/profile_activation_tests.cpp`'s scratch directory is
inside `#if !defined(_WIN32)`, so MSVC never compiles that read. The gate
reads text rather than branches (`tests/scripts/msvc_secure_crt_tests.py`), so
the read now carries the scoped suppression the tree spells everywhere else
(`tests/application/mcp_context_helper_tests.cpp` ~66), with a comment saying
why a POSIX-only class carries it. Nothing else in the file changed;
`clang-format` moved the `pattern` line out of its alignment group.

## Item 4: central Python discovery

`tests/end_to_end/CMakeLists.txt` no longer calls
`find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)`. That call was
also the only thing that defined `Python3_EXECUTABLE` in this directory: two
rows read `"${Python3_EXECUTABLE}"`, and with the call removed both configured
with an empty program (`CMake Warning (uninitialized)` at each). Both now name
the central `Python3::Interpreter` target, as the other Python rows in this
file already do:

- `application_epoch_portable_occurrence_two_catalogues` (the row at the
  reported line);
- `application_epoch_ordinary_update_recreated`.

`ctest -N -V` shows both commands resolving to
`/home/mmarcin/.local/bin/python3`, the interpreter the central discovery
found. The configure has 124 `CMake Warning (uninitialized)` blocks
(`0854-configure2.log`), the same count as the configure before this change
(`configure-dev.log`), and all of them are inside Qt's and
KSyntaxHighlighting's own package files. The intermediate state, with the
call removed and the variable still read, had 126: the two extra were the two
rows above.

## Verification of items 2 and 4 (Linux, bpr-linux, WSL2)

Stock `dev` preset, reconfigured and fully rebuilt with no warning. The Secret
Service `login` collection read `(<false>,)` before the group. Each run was a
serial `ctest --test-dir build/dev -j1 -V -R '^<name>$'` from 17:45:25Z
(`0854-bc.summary`, `0854-bc-<row>.log`).

| Row | Result | Time |
|---|---|---|
| `msvc_secure_crt_contract` | Pass | 0.32 s |
| `sqlite_profile_activation` | Pass | 4.14 s |
| `dependency_modernization_contract` | Pass | 65.53 s |
| `application_epoch_portable_occurrence_two_catalogues` | Pass | 11.92 s |
| `application_epoch_ordinary_update_recreated` | Pass | 4.40 s |

Gates: `/usr/bin/clang-format` 22 reports the changed C++ file clean, and
`git diff --check` is clean.

## Item 1: the thirteen `tempfile` sites

All thirteen now call `scripts/contained_temporary_support`, through the idiom
the tree already uses (`sys.path.insert` of the repository root, the BPR-0350
comment, and `from scripts.contained_temporary_support import ...`). The
support module itself is unchanged: it is outside this station's authority for
this task, and nothing required a change there.

- **The two [[BPR-0847]]/[[BPR-0849]] courses.** `private_fixture_parent()`
  keeps its exact shape, including `profile_owner_admission_contract.py`'s
  `yield None` for `os.name != "posix"` and its POSIX-guarded parent
  assertion. On POSIX the helper delegates to `tempfile.mkdtemp`, so the
  parent is still created mode `0700` and the owner-only check still passes.
  The negative controls below show it still fails by name under a shared
  parent.
- **The stderr sink.** `device_binding_relaunch_contract.py`'s
  `tempfile.TemporaryFile()` becomes a real file opened inside a contained
  temporary directory. The child still gets an operating-system handle, the
  not-ready path still `seek(0)`s and reads it, and the file is closed before
  the directory is removed.
- **The held object.** `rfc0015_cli_publish_fixture_tests.py` uses
  `ContainedTemporaryDirectory`, whose `name` and `cleanup` are what that
  `setUp`/`addCleanup` pair already used.
- **The rest** are single-site swaps, with `dir=` becoming `parent=`.

### Verification of item 1 (Linux, bpr-linux, WSL2)

Each row was its own serial `ctest --test-dir build/dev -j1 -V -R '^<name>$'`
(`0854-python/`).

| Row | Result | Time |
|---|---|---|
| `contained_temporary_usage_contract` | Pass, 6 of 6 cases | 2.65 s |
| `application_device_binding_relaunch` | Pass, `DEVICE_BINDING_RELAUNCH_OK` | 7.54 s |
| `application_profile_activation_public_path` | Pass, `PROFILE_ACTIVATION_OK` | 2.75 s |
| `application_profile_owner_admission_public_path` | Pass, `PROFILE_OWNER_ADMISSION_OK` | 1.40 s |
| `application_profile_window_restart_public_path` | Pass, `PROFILE_WINDOW_RESTART_OK` | 5.79 s |
| `application_profile_window_layout_public_path` | Pass, `PROFILE_WINDOW_LAYOUT_OK` | 23.82 s |
| `application_profile_window_review_restoration_public_path` | Pass, `PROFILE_WINDOW_REVIEW_RESTORATION_OK` | 98.61 s |

Negative controls, with `BPR_TEST_SOCKET_ROOT=/tmp`: both the activation and
the owner admission course still fail by name with
`/tmp is not owner-only, so an owner's workspace recovery would refuse`
(0.18 s and 0.04 s).

The three files with no ctest row were run directly. The runs were made
inside a network namespace with only a down loopback, so no GitHub contact was
possible; that wrapper is in the run record rather than in the logs, and the
reviewer reproduced two of the probe's modes the same way:
`rfc0015_cli_read_fixture_tests.py` (3 tests OK),
`rfc0015_cli_publish_fixture_tests.py` (6 tests OK), and
`probe_gh_clone_git_bridge.py` in four modes against the local `gh` 2.97.0 and
`git` 2.53.0 (all exit 0). The probe's `--sandbox-config` mode needs macOS's
`sandbox-exec` and was not run; it does not touch the changed line.

### Windows, which this station cannot run

Five of these rows register without `WINDOWS_UNCONTAINED`, so on Windows they
run inside the AppContainer, which is the case the support module exists for.
The course root is now created by `os.mkdir` with no mode, inheriting
`%TEMP%`'s entries, instead of CPython's protected owner-only descriptor. A
Windows run has to confirm that the contained driver can use the directory it
just created, that `make_storage_root_owner_private`
(`src/adapters/sqlite/src/catalog_database.cpp` ~4803, called from
`src/app/main.cpp` ~7653) accepts and narrows that root, that each course
still prints its `_OK` marker, and that the relaunch course's stderr file is
written, read back and removed without a sharing violation.
`application_profile_activation_public_path` is registered under `if(UNIX)`,
so Windows is unaffected there.

## Item 3: the conditional-branch findings are all gate false positives

**The card's premise was wrong** and is corrected here: the eleven findings
are not in branches this host does not compile. All eleven sit under
`#if defined(BPR_BUILD_TESTING)`, and this build compiles both translation
units with `-DBPR_BUILD_TESTING=1`. The gate's `host_macros()` returns
platform macros only, so it treats every testing-only region as dead code it
must police.

- **`src/app/main.cpp:8621` is not a misspelling.** The file declares a
  function `schedule_native_notification_run` (`:7289`) and, in `main`, a
  local flag `scheduleNativeNotificationRun` (`:8607`/`:8610`). The call at
  `:8621` is the function; the flag guards it. Both are correct, both are
  required, and the gate's `spelling_key` folds away exactly the underscores
  and case C++ uses to tell them apart. Twelve lines lower the same file does
  the same thing in a region the gate does not treat as skipped
  (`scheduleAutomatedRun` and `schedule_automated_run`). Renaming the call
  would make it call a `const bool` and orphan the definition, so **no
  product file changed and the round-trip pins are untouched.**
- **The ten `github_application.cpp` findings are lambda init-captures**
  (`options`, `config`, `moves`, `accepted`). An init-capture declares a name
  with no type in front of it, so the gate's `DECLARATION` pattern cannot see
  it; the name then reads as an undeclared use that `spelling_key` folds onto
  the parameter it was initialized from. The gate's `declared_in` already
  understands capture lists, but only the bare-parameter pass consults it.

Fixing this needs `scripts/verify_conditional_branches.py`, which is outside
this station's authority for this task. So **this row stays red at this head**,
and the two patches below are proposed, not applied. They were verified in a
throwaway detached worktree at `e967236e`: with both rules the gate reports
`CONDITIONAL_BRANCH_IDENTIFIERS_OK:695` over the whole tree, well inside the
row's 60 s timeout; with the
init-capture rule alone it reports `FAILED:1` (`main.cpp:8621`); with the
call-position rule alone `FAILED:10`; and four planted genuine misspellings in
copies of the two real files are still caught, including one inside an
init-capture's initializer, one in a lambda body, one call to a name nothing
defines, and one call whose only same-spelling declaration is a variable. The
patches, the patched files and the logs are in
`/home/mmarcin/bpr-station-logs/task-20260917-lnx-sweep-fixes/0854-gate/`
(`patch-1-verify_conditional_branches.diff`,
`patch-2-conditional_branch_identifier_tests.diff`, `20-` to `28-*.log`), and
the exact patch text is in this station's return to main.

## For macOS and Windows

- **Still needed on macOS:** `contained_temporary_usage_contract`,
  `msvc_secure_crt_contract` and `dependency_modernization_contract` pass, and
  the courses whose files changed still pass.
- **Still needed on Windows:** the five contained courses above, and
  `msvc_secure_crt_contract` and `contained_temporary_usage_contract` where
  they are registered.
- **Still open for main:** `conditional_branch_identifiers` stays red until the
  gate patch lands on a checkout that owns `scripts/`.

## Independent review

An `rfc-reviewer` reviewed the whole candidate (`0854-candidate-code.diff`,
sha256
`e7e6bc37043165ed8d14d60b0a7aed7ab468bef0fd84ff0ff6b1ea886e8b811f`) and found
no blocking finding. It re-ran the gates and rows it could, and confirmed
independently that:

- no `tempfile` creation call remains under `tests/`, the repository root is
  on `sys.path` for both test directories whatever the working directory, no
  helper call passes a positional or `dir=`, and the POSIX helper is
  `mkdtemp` at mode `0700`;
- the owner-only parent contract and the Windows branch are unchanged, and the
  relaunch course's child is reaped before its stderr file is closed and that
  file is closed before its directory is removed;
- the MSVC suppression sits inside the gate's lookback window and matches the
  tree's existing idiom;
- nothing under `tests/end_to_end` still reads the variable the removed
  `find_package` defined, no `find_package(` remains under `src/` or `tests/`,
  and `bpr_add_network_isolated_test` refuses an empty command, so a row
  cannot silently register with no program again;
- `src/app/main.cpp` and `src/app/src/github_application.cpp` are byte-identical
  to `HEAD`, `scripts/` is unmodified, `main.cpp:8621` is not a misspelling,
  and the ten other findings are init-captures.

Its notes were taken up above: the warning-count comparison now names the
before log, the count of sibling Python rows is no longer stated as a number,
and the probe's network-namespace wrapper and the gate's runtime are now
attributed to the run record rather than to the logs. One further note stands
unchanged in the code: the contained helper swallows a removal error where
`tempfile.TemporaryDirectory` would raise. No course asserts on removal and
every `_OK` marker prints before the directory is removed, so on POSIX this
cannot turn a failure into a pass; on Windows it would leak a directory rather
than fail the row, which the Windows run above should watch for.
