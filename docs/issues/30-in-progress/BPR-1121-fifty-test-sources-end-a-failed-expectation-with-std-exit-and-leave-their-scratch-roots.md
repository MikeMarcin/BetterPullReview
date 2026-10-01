---
schema: "implement-rfc.card.v1"
id: "BPR-1121"
title: "Fifty test sources end a failed expectation with std::exit and leave their scratch roots, as sqlite_profile_activation did"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux", "mac", "win"]
metadata: {"found_by":"BPR-1118's source scan (its 'Seen elsewhere, not changed here' section), re-scanned at HEAD 45191097","reserved_title":"Thirty test sources end a failed expectation with std::exit and leave their scratch roots, as sqlite_profile_activation did","related_cards":["BPR-1118","BPR-1113","BPR-1051","BPR-1103","BPR-0803"],"reservation":"docs/issues/_reservations/BPR-1121--BPR-1121--2eea9dcdb3a9.json"}
---

# BPR-1121: Fifty test sources end a failed expectation with std::exit and leave their scratch roots, as sqlite_profile_activation did

## Why

[[BPR-1118]] fixed one test source, `sqlite_profile_activation`. Its `expect`
printed `FAILED:` and then called `std::exit(EXIT_FAILURE)`, which runs no
destructor of a local object. So every failing run left its `/tmp` scratch
roots behind. The fix made `expect` throw and `main` catch it, so a failure
unwinds through the owners that remove the roots.

That card's scan found thirty other sources with the same shape. The title
reserved for this card says thirty. This station saw one of them leak on
2026-09-24: `application_device_flow_cli_choice` failed on Linux (see
[[BPR-0803]]) and left `/tmp/device_flow_cli_choice-nsdREo`, a whole
application data root, because its `QTemporaryDir` is never destroyed.

## The re-scan at HEAD (`45191097`)

**Method.** A test source is in the class when a failure helper in it
(`expect`, `require`, `fail` or a one-off site) ends with `std::exit`, and
it makes a root in the system temporary directory:
`std::filesystem::temp_directory_path()`, `mkdtemp`, `QTemporaryDir` or
`QDir::tempPath()`.

- **Fifty-two sources match.** [[BPR-1118]]'s thirty are the ones that use
  `temp_directory_path` or `mkdtemp`. Its scan did not count
  `QTemporaryDir`, which leaks the same way: twenty-two more sources.
- **Two of the thirty are false positives**, so the class is fifty:
  - `tests/application/mcp_context_helper_tests.cpp` keeps its scratch in a
    function-local static, which `std::exit` does destroy ([[BPR-1051]]).
  - `tests/application/epoch_application_host_tests.cpp` only names a path
    under the temporary directory and creates nothing there.
- **Seen, not in the class:** `tests/sqlite_contract/storage_root_tests.cpp`
  and `appcontainer_private_descriptor_tests.cpp` also end with `std::exit`,
  but their scratch sits under a directory the registration passes, in the
  build tree.

The fifty, by directory, with the rows that run them on Linux:

| Directory | Sources | Roots |
| --- | --- | --- |
| `tests/sqlite_contract/` | `anonymous_epoch_authority`, `appearance_settings_storage`, `contained_catalog_probe`, `device_binding_storage`, `epoch_catalog_behavior`, `epoch_catalog_storage`, `private_store_open`, `profile_registry_storage`, `profile_runtime_lease`, `profile_storage_root`, `queue_enrollment_storage`, `release_maintenance_files`, `review_ledger_storage`, `review_mutation_recovery_storage`, `review_submission_recovery_storage`, `window_review_restoration_storage`, `workbench_settings_storage` (17) | scratch classes; two bare roots in `main` |
| `tests/git_contract/` | `anonymous_public_read`, `epoch_maintenance_integration`, `git_cache_contract`, `git_process_contract`, `haves_trimmed_pack_integration` (5) | `temporary_root` classes |
| `tests/github_contract/` | `gh_cli_transport`, `github_cli_process_runner`, `live_review_submission_provider`, `gh_cli_git_smart_transport` (macOS only) (4) | classes and `QTemporaryDir` |
| `tests/application/` | `designated_review_authority_bootstrap` (1) | a scratch class |
| `tests/adapters_qt/` | `agent_launch_canary`, `markdown_body_view` (2) | bare directories and marker files; `QTemporaryDir` |
| `tests/end_to_end/` | `application_logging`, `attended_recent_replay`, `device_binding_launch_renewal`, `device_binding_restart`, `device_flow_cli_choice` (macOS row), `github_application_liveness`, `personal_discovery_public_path`, `profile_empty_desktop_native_menu_bar` (macOS only), `profile_management_public_path`, `profile_multi_window_public_path`, `profile_second_window_close_public_path`, `profile_startup_public_path`, `profile_two_account_isolation_public_path`, `release_maintenance_public_path`, `release_update_public_path`, `release_update_recovery`, `release_update_transaction` (macOS only), `setup_destination_public_path`, `windows_profile_storage_startup_contract` (Windows only) (19) | `QTemporaryDir` data roots; one scratch class |
| `tests/logging/` | `quill_logging` (1) | `QTemporaryDir` |
| `tests/syntax_contract/` | `syntax_classification_contract` (1) | a fixed-name canary directory |

Four of the fifty are built only on macOS or Windows, so this station
cannot compile them.

## What a throwing `expect` must not do

A failure that unwinds is not always safe where `std::exit` was. The audit
found these hazards in the fifty:

- **A helper thread still running.** A joinable `std::thread` destroyed while
  unwinding calls `std::terminate`. A `std::jthread` joins, and some of them
  wait on a condition the failing path never sets, so the join never
  returns. For example, `git_process_contract` has `require` calls while an
  `index_pack` thread is blocked on a cancellation that comes after them.
- **A forked child.** After `fork` the child's thread is the parent's, so a
  throw in the child unwinds into the parent's frames. It would run the
  parent's destructors, including the one that removes the shared root.
  Five of the sqlite sources, `git_process_contract`,
  `github_cli_process_runner` and `designated_review_authority_bootstrap`
  fork.
- **A destructor.** Four `direct_database`/`fake_cache_session` destructors
  call `expect`. A throw from a destructor calls `std::terminate`.
- **A Qt event dispatch.** A throw from a slot or an event handler is
  undefined behaviour in Qt. The device-flow course's consent timer can
  reach `expect`, and two courses call it in startup and quit callbacks
  that the event loop delivers.
- **Code under test that catches it.** Product code has several hundred
  `catch (...)` handlers, and some courses call `expect` from hooks the
  product invokes. A caught throw would let the course run on and possibly
  pass.

## Scope

- **One shared helper**, `tests/support/test_expectation.hpp`, replaces each
  source's own `std::exit` ending, grouped by the helper the sources share:
  - `expect` prints the `FAILED:` line and throws;
  - `fail()` ends a failure whose line the caller has already printed;
  - `main` hands anything that reaches it to one handler, which names an
    unexpected exception and returns the failing exit.
- **It unwinds only where that is safe.** On any other thread, in a forked
  child, while an exception is already unwinding, or inside a scope marked
  `exit_on_failure`, it ends the process with `std::exit` as before.
- **A caught failure still fails.** If an expectation's exception is
  swallowed, the run still exits with failure. A failure that does not
  finish unwinding within a bound ends the process instead of hanging.
- **Every root is owned from the moment it exists.** That covers a scratch
  root whose constructor fails part-way, and the bare roots in `main`.
- **Each hazard above is marked or made safe**, and listed here.
- **Record what still leaves a root**: a kill, a timeout, a crash, and each
  marked scope.

## Acceptance

- AC1 The fifty sources end every failed expectation through the shared
  helper. No source in the class ends one with its own `std::exit`, and the
  exceptions are the hazard scopes listed on this card.
- AC2 Trials that force an expectation to fail while a root exists leave no
  root for every Linux row of the class. The row still fails with its
  `FAILED:` line. The same trials at `HEAD`'s sources, with `std::exit`,
  leave the roots. That run is the negative control that shows the trial
  can see a leak.
- AC3 An exception from the standard library fails the row with a named
  `FAILED:` line and removes the roots.
- AC4 A marked hazard scope still ends with `std::exit`, and does not hang. A
  swallowed expectation still fails the run. A trial shows each of these.
- AC5 The passing rows are unchanged on Linux: every Linux row of the class
  passes, and prints the same lines as at `HEAD`.
- AC6 The change is portable: the sources build and their rows pass on
  macOS and Windows. The four sources built only there are compiled first
  there.
- AC7 Gates: `/usr/bin/clang-format` 22, `git diff --check`,
  `msvc_secure_crt_contract`, `DOCUMENTATION_OK`, and no scoped `tidy.py`
  finding on a changed line. Revised 2026-09-24, before the change was
  committed: the fifty sources carry tidy findings on lines this card does
  not touch, so `--warnings-as-errors` over whole files cannot pass.

## Boundaries

- Test sources and one test-support header only. No product source, no row
  registration and no composition pin changes.
- Every run is through `ctest`, offscreen, with `DISPLAY`, `WAYLAND_DISPLAY`
  and `DBUS_SESSION_BUS_ADDRESS` unset, and with fresh absolute `HOME` and
  XDG roots. Nothing is created in `/run/user/1000`.
- A trial's own leftovers are found by a listing of `/tmp` before and after
  the run. They are removed by exact name only if this user owns them and
  their name has a root's shape. Nothing else in `/tmp` is removed.

## Fixed at `98dbf662`, verified on Linux — bpr-main-linux, 2026-09-24

Claimed at `172330e7`. The change is one commit, `98dbf662`. It adds
`tests/support/test_expectation.hpp` and changes the fifty sources, and
nothing else: no product source, no registration and no composition pin.

It was verified on `08d3ab0c` plus the change. It was then rebased onto
`32af9f3f` and its passing rows run again there. `98dbf662` is that change
rebased once more, over commits that touch no path under `src/`, `tests/`
or `cmake/`. Mac batch 8 and the owner's
corrections added to two sources in the class, and both are included, as the
coordinator asked:

- `device_binding_launch_renewal` gained four more `QTemporaryDir` data
  roots, two from [[BPR-1116]] and two from [[BPR-0644]]'s `518fcc37`;
- [[BPR-1111]] gave `device_binding_storage` new courses under its root.

### The shared helper

`tests/support/test_expectation.hpp`, in `bpr::tests`:

- `expect(condition, message)` and `fail(message)` print `FAILED: <message>`
  and throw `expectation_failed`. `fail()` does the same for a caller that
  printed its own line. `expectation_failed` is not a `std::exception`.
- Each `main` is a function try block. Its handler returns
  `failing_exit()`, so every owner in `main`'s body is destroyed first. Any
  other exception is named: `FAILED: unexpected exception: <what>`. Before,
  it ended in `std::terminate`.
- A failure still ends the process with `std::exit`, as before, when
  unwinding is not safe. The helper decides that itself when the failure is
  on another thread, in a forked child (the process id changed) or while an
  exception is already unwinding. A scope decides it by declaring
  `exit_on_failure`.
- Two backstops keep a failure a failure:
  - An expectation whose exception the code under test catches is still
    reported at exit: `FAILED: an expectation failed and its exception was
    caught before main`, and the exit code becomes 1.
  - A process still running 20 s after a failure is ended with a named line.
- `scratch_guard` removes a path unless it is released. A scratch root's
  constructor holds one from the moment the root exists until it is fully
  built. A root made without an owner holds one for its whole life.
- `end_forked_child(work)` runs a forked child's work and ends the child
  with `_exit`, however the work ends. So a child never unwinds into its
  parent's frames.

### The fifty, by the helper they share

- **The failure helper.** 42 sources had a local `expect` that exited. It is
  now `using bpr::tests::expect;`. Six of them printed `FAIL:` or a bare
  message, and now print `FAILED:`. Five `git_contract` sources keep their
  local `require` name, which forwards to the shared `expect`. Three sources
  are hand edits:
  - `github_application_liveness` uses the shared `expect` and `fail`;
  - `profile_empty_desktop_native_menu_bar` keeps its menu-trace
    diagnostics, then calls `fail`;
  - `private_store_open` already had a function try block, and its handler
    is now the shared one.
- **One-off exits.** Six sites printed their own lines and then exited. They
  now call `bpr::tests::fail()`: the two SQLite `execute` helpers, three
  sites in the syntax contract, and `fail_unscripted` in
  `live_review_submission_provider`.
- **`main`.** Every `main` in the class is a function try block. Eleven used
  to fall off their end, and now return `EXIT_SUCCESS` explicitly.
- **Roots owned from the moment they exist.**
  - 19 scratch constructors, one per source, hold a `scratch_guard` until
    they are built: most of the sqlite and git roots, and the bootstrap,
    submission, gh CLI and Windows storage roots. The bootstrap's has one
    guard in each platform branch.
  - Bare roots now hold one for their whole life:
    - `device_binding_storage`'s root in `main`;
    - `contained_catalog_probe`'s root, which its early `return EXIT_FAILURE`
      paths also leaked;
    - the agent canary's working directory and descendant marker file, and
      its two Windows-only paths.
  - Two destructors that could throw while a failure unwinds now use the
    `error_code` form of `remove_all`: `profile_registry_storage` and
    `window_review_restoration_storage`.
- **Forked children.** Eight sites in six sources run the child through
  `end_forked_child`: `device_binding_storage`, `profile_runtime_lease` (2),
  `queue_enrollment_storage`, `release_maintenance_files`,
  `review_ledger_storage` (2) and `designated_review_authority_bootstrap`.
  An exception in a child used to call `std::terminate`. Once `main` catches
  everything, it would instead have unwound into the parent's frames in the
  child. `git_process_contract` and `github_cli_process_runner` fork only
  inside their fake `git` and `gh` child processes, which own no root, so
  they are unchanged.

### The scopes that still exit (`exit_on_failure`)

A failure in these ends the process as before, and can leave a root.

| Kind | Where |
| --- | --- |
| A destructor | the `direct_database` destructors of `anonymous_epoch_authority`, `epoch_catalog_behavior` and `epoch_catalog_storage` |
| A joinable `std::thread` or blocked call alive | `git_process_contract`: the queued index-pack calls, and the capability-probe and boundary-lock waiters. `github_cli_process_runner`: the queue blocker and the watchdog caller |
| Qt event dispatch | `device_flow_cli_choice`'s consent timer, `release_maintenance_public_path`'s startup completion, `release_update_public_path`'s quit completion, `profile_second_window_close`'s menu poll |
| Code under test that catches and reports | `live_review_submission_provider`'s fake transport and `fail_unscripted`, as [[BPR-0904]] requires. The agent launcher's event sink in the canary's parent-death course |

Any other hook the product calls is left to the backstops. Examples are
`gh_cli_transport`'s fake process port under the mutation session, and the
git cache's witnesses. The failure's own `FAILED:` line comes first, and the
run fails.

### Verification on Linux

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2. The candidate was built in
`build/dev` (Debug, `-Werror`) with no `warning:` line. `HEAD`'s sources were
built in a worktree under `build/bpr-1121-trial/`, over the same dependency
cache: first at `5e2591de`, then at `08d3ab0c` for the trials below, and at
`32af9f3f` for the final comparison. The worktree was removed afterwards. Every run went through `ctest`, serially and offscreen,
with `DISPLAY`, `WAYLAND_DISPLAY` and `DBUS_SESSION_BUS_ADDRESS` unset and
with fresh absolute `HOME` and XDG roots. None added a `bpr-` name to
`/run/user/1000`.

**How the trials force a failure.** A trial hook, never committed, counts
expectations per process. It makes the Nth one fail
(`BPR_TRIAL_FAIL_AT`), throw `std::runtime_error` (`BPR_TRIAL_THROW_AT`), or
fail the first one whose message contains a given text
(`BPR_TRIAL_FAIL_MESSAGE`). In `HEAD`'s sources it sits in each file's own
helper, and in the candidate in the shared `expect`, so both variants count
the same calls. A counting run gave each row's total. The trials then failed
each row at a third and at two thirds of it, with the same N for both
variants. The roots a run leaves are the root-shaped `/tmp` entries it added
that this user owns. Each is removed by exact name after the run.

| Claim | Trial | `HEAD` (`std::exit`) | Candidate |
| --- | --- | --- | --- |
| AC2 | 102 forced failures over the 51 rows that count (below) | 91 leave roots. The other 11 fail before any root exists | 1 leaves a root, the artifact below. Every run fails with its `FAILED:` line |
| AC2 | a failure while a root certainly exists: markdown's resource directory, the syntax canary, the canary's descendant marker, and the launch-renewal course's new in-process data root | 4 of 4 leave roots | 0 of 4 |
| AC2 | at `32af9f3f`, a failure inside the launch-renewal course's account-mismatch phase (`a fresh restored grant spends nothing at the token endpoint`) | leaves 2 roots: the course's and the phase's own | none left |
| AC2 | the bootstrap's half-built root, N = 1042 (`set exact owner-private bootstrap directory permissions`) | leaves it | removed |
| AC3 | `std::runtime_error` at a third of 9 rows, one per group | 8 abort (exit -6) with no `FAILED:` line. In `device_binding_storage` the throw lands in the forked child, which aborts, and the parent fails. 7 leave roots | exit 1 with `FAILED: unexpected exception: trial exception`, from the child through `end_forked_child` in `device_binding_storage`. None left |
| AC2 negative control | the candidate with unwinding turned off (`BPR_TRIAL_NO_UNWIND`), same 9 rows and N | — | 7 leave roots, as at `HEAD` |
| AC4 | a failure in each kind of marked scope: a thread window in each runner, a destructor, the event loop, the session's transport | exit 1 | exit 1 at once: no abort and no hang, 0.0 to 6.5 s. The root is left, as recorded above |
| AC4 | a failure on a worker thread (`quill_logging`'s writers) | exit 1 | exit 1, as before |
| AC4 | a failure in a forked child (`device_binding_storage`'s `seed`) | the child exits, the parent exits, the root is left | the child exits, the parent unwinds, the root is removed |
| AC4 | a failure the mutation session catches (`gh_cli_transport`) | exit 1 | the course runs on to the end. At exit, `FAILED: an expectation failed and its exception was caught before main` and exit 1. The row fails |

**The one candidate leftover is an artifact of forcing.** N = 1041 made
`expect(create_directory(Root), "create owner-private bootstrap directory")`
report false after the directory had been made. In reality that check fails
only when nothing was made, or when the name was already someone else's.
The guard begins after it on purpose, so that a root the course did not
create is never removed. N = 1042, one expectation later, is the real
half-built case, and the candidate removes it.

**What the counting runs showed.** The candidate evaluated as many
expectations as `HEAD` in every row but two. The two
`git_haves_trimmed_pack` rows differ by a few (187 against 190, and 151
against 146), and two `HEAD` runs of them differ as much (184 and 190, 147
and 146). Five rows print no count because a Python driver runs their
executable and keeps its output: `agent_parent_death`, both
`epoch_*kill_recovery` rows, `live_review_submission_restart_recovery` and
`quill_logging_backpressure`. The modes those drivers run work in a root the
driver owns, and make none of their own. `quill_logging_backpressure`'s
counting run fails on both variants, because the hook's own count line
blocks on the stderr pipe that the driver deliberately fills. Without the
hook, the row passes.

| Claim | Check | Result |
| --- | --- | --- |
| AC5, the passing rows | all 56 Linux rows of the class, verbose, at `HEAD` and at the candidate, on `08d3ab0c` and again on `32af9f3f` | 56 of 56 pass each time. At each base every row prints the same lines as `HEAD`: 51 byte for byte, and 5 differ only in a random temporary name or a millisecond timing |
| AC7 | `/usr/bin/clang-format` 22 `--dry-run -Werror` on the header and the fifty sources; `git diff --check` | Pass |
| AC7 | `coding_format_check`, `msvc_secure_crt_contract` and `architecture_boundaries` at the candidate | Pass. `architecture_boundaries` first failed on the trial worktree under `build/`, which its scan reads. With the worktree removed, it passes |
| AC7 | `_coding-standard/scripts/tidy.py` on the header and the 46 sources this station builds | 21 findings, all on lines this change does not touch, and none in the header |
| AC6, partly | a syntax-only compile of the four sources built only elsewhere, with a neighbouring target's flags | `gh_cli_git_smart_transport` and `release_update_transaction` pass. The Windows source needs `windows.h` and the cocoa one Qt's private QPA headers |

The logs are station records and are not committed. In
`build/logs/bpr-1121/`:

- the candidate's passing runs, `candidate-final-verbose.log` at `08d3ab0c`
  and `candidate-final-32af9f3f-verbose.log`;
- `candidate.patch` and `candidate2.patch`, the exact trees tested;
- the gate logs `gates-*.log` and `tidy-candidate.log`, and `build-*.log`.

In `build/logs/bpr-1121/trial/`:

- the hook and its scripts;
- `HEAD`'s passing runs, `head-baseline-08d3ab0c.log` and
  `head-baseline-32af9f3f.log`;
- `head/` and `candidate/`, with their `results.json`;
- the targeted logs, including the mismatch-phase pair.

### What still leaves a root

- a kill, which includes a CTest timeout;
- an abort or a crash;
- a failure inside one of the `exit_on_failure` scopes above;
- a failure on a thread other than the one that runs `main`;
- a failure that the 20 s bound cuts off;
- on Windows, a course whose root `remove_course_root` has to retry
  ([[BPR-1103]]). A failure destroys the course's `QTemporaryDir` once, and
  Windows refuses to remove a store file that is still open.

### Seen, not changed

- Four sqlite storage contracts name their scratch roots `bpr-<store>-<N>`
  from a per-process counter: appearance, queue, ledger and workbench. They
  `remove_all` that path before creating it. So a run removes whatever is at
  that name, even if another run made it. [[BPR-1126]] fixed them at
  `ad117024`. It also moved the syntax contract's control root, in this
  card's class, from a fixed `/tmp` name to a `QTemporaryDir`.
- Optional, with no card: `storage_root_tests` and
  `appcontainer_private_descriptor_tests` still end a failure with
  `std::exit`, but their scratch is in the build tree, not in `/tmp`.

### Evidence owed elsewhere (AC6)

Linux compiles none of the four sources that are built only on macOS or
Windows. From the macOS station:

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R '^(sqlite_|git_|gh_cli_|github_cli_process_runner|live_review_submission_|qt_agent_launch_canary|agent_parent_death|qt_markdown_body_|application_(designated_review_authority_bootstrap|logging|attended_recent_replay|device_binding_|device_flow_cli_choice|poller_liveness|personal_discovery_public_path|profile_|release_|selected_file_safety_public_path|setup_destination_public_path)|epoch_|contained_catalog_probe|quill_logging|release_update_|syntax_classification_contract)'
```

`application_profile_empty_desktop_native_menu_bar` runs only in the owner's
Aqua session, so the station builds it and leaves the run to that session.
From the Windows station, the same selection, including
`application_windows_profile_storage_startup_contract`. On each station, one
forced failure while a root exists should show the root removed. The card
stays In Progress on this station's claim until those runs are supplied.

## macOS, AC2 and AC6 (bpr-main-mac, 2026-09-24)

Station `bpr-main-mac`: macOS 26.6.2 (25G83) on Apple Silicon, Apple clang
21.0.0 (`clang-2100.1.1.101`), Qt 6.11.2, `build/dev` Debug with warnings
as errors. This entry supplies the macOS runs that AC6 owes. It does not
move the card, which stays on bpr-main-linux's claim. The Windows run is
still owed.

### The build and the four sources

At `be8b0fad`, which contains `98dbf662`, `cmake --build --preset dev`
exited 0 after 167 steps, 56 of them compiles. It printed no `warning:`
line.

| Source built only elsewhere | macOS |
| --- | --- |
| `tests/github_contract/gh_cli_git_smart_transport_tests.cpp` | compiled and linked in that build, no warning |
| `tests/end_to_end/release_update_transaction_tests.cpp` | compiled and linked in that build, no warning |
| `tests/end_to_end/profile_empty_desktop_native_menu_bar_tests.cpp` | compiled and linked in that build, no warning, with Qt's private QPA headers |
| `tests/end_to_end/windows_profile_storage_startup_contract_tests.cpp` | not compilable here. It includes `<windows.h>` at line 31, outside any `#if`, and is registered only under `WIN32` (`tests/end_to_end/CMakeLists.txt:2705`). A syntax-only compile with a neighbouring target's flags stops at `fatal error: 'windows.h' file not found`. It stays owed to Windows |

### The rows

The card's command, serial and verbose, with the native menu-bar row left
out (`-E '^application_profile_empty_desktop_native_menu_bar$'`), ran from
02:39:43Z to 03:16:11Z. On macOS the selection matches 80 rows. These
include the macOS-only rows and some rows outside the class that the same
patterns match.

| Check | Result |
| --- | --- |
| 79 rows, at `be8b0fad` | **79 of 79 passed**, in 2,188.30 s. Among them are `gh_cli_git_smart_transport` (11.76 s), `release_update_transaction` (0.53 s) and `application_device_flow_cli_choice` (22.54 s). The slowest, `application_profile_window_review_restoration_public_path` (595.52 s of its 1,800 s timeout), is outside the class |
| `application_profile_empty_desktop_native_menu_bar` | **not run.** It sets `QT_QPA_PLATFORM=cocoa` and needs the owner's Aqua session. It was built |
| `/private/tmp` and the user's `$TMPDIR`, listed before and after | `/private/tmp` gained no entry. `$TMPDIR` gained only Chrome's and a desktop indexer's files |

On macOS a ctest run cannot show a leftover ([[BPR-1047]]). The runner
gives the row its own `/tmp/bpr-net-*/fixture` as `TMPDIR` and removes it
when the row ends. The trial below is what shows whether roots are removed.

### A forced failure while a root exists

**Method.** A scratch driver, not committed, ran each row's command
directly, as [[BPR-1051]]'s macOS run did. It read the row from
`ctest --show-only=json-v1`. It made the runner's layout but kept it: a
root `bpr1121-direct-XXXXXXXX/` in this session's scratch directory, and a
`fixture/` child, mode `0700`, as `TMPDIR`. It wrote the profile with the
runner's own `write_profile()`. It gave the child the row's environment and
environment changes and the runner's four variables. It ran
`/usr/bin/sandbox-exec -f <profile> <command>` from the row's working
directory, judged the row by CTest's rule, and then listed `fixture/`
three levels deep.

**The trial hook** is Linux's, rebuilt here and never committed. It sits
in the shared `expect`. `BPR_TRIAL_FAIL_MESSAGE` fails the first
expectation whose message contains the text. `BPR_TRIAL_NO_UNWIND` makes
`fail()` take its `std::exit` branch, which is the negative control that
shows the trial can see a leak. The header's SHA-256 was `1a2cdc9d…c6c25`
before, `aee74901…edba` with the hook, and `1a2cdc9d…c6c25` again after
the restore. `git diff` was then empty. The header was touched, because
the copy kept its old time, and the 49 dependent compiles were redone
with no warning. `strings` finds no `BPR_TRIAL` in the rebuilt
executables.

The trials ran at `b8944a06`. Neither trial source nor the header differs
from `98dbf662`.

| Row, and the expectation failed | Root that exists at that point | Candidate | `std::exit` (control) |
| --- | --- | --- | --- |
| `qt_markdown_body_1_0`, `negative control: default Qt document reads the existing file` (the 25th) | the resource `QTemporaryDir`, holding `canary.png` | exit 1, the `FAILED:` line, `fixture/` empty | exit 1, the same line, and it left `better_pull_review_markdown_body_view_tests-XHqXMC/canary.png` |
| `application_device_binding_restart`, `first refresh remains deferred after resume` (the 12th) | the course's `QTemporaryDir` data root, with the second application running on it | exit 1 after 6.68 s, the `FAILED:` line, `fixture/` empty | exit 1, and it left `device_binding_restart-lLWaVm/` with `data/` (`agent-workspace-ownership-v1/`, `logs/`, `state/`) and `data-agent-workspaces-v1/` |

The same hook also failed five more rows of the class for [[BPR-1126]]:
the four sqlite storage contracts and `syntax_classification_contract`.
Each left no root, and each control left one. That card has the
details.

Unforced, the two rows passed by the direct route with `fixture/` empty.
They passed before the hook, and again after the restore. They also
passed through ctest after the restore. `/private/tmp` gained no entry in
any run. The kept roots were removed by exact name afterwards.

The logs, the driver, the hook's patch and the per-run JSON are station
records under `build/logs/bpr-1121-mac/` and are not committed.

### Acceptance state after this run

- **AC6 on macOS: met, except for one row that needs the owner.** The
  three macOS-only sources compile with no warning, and 79 of the 79 rows
  run here pass. `application_profile_empty_desktop_native_menu_bar` was
  built but not run, because it needs the owner's Aqua session. The
  Windows-only source and the Windows rows are still owed.
- **AC2, repeated on macOS:** a failure while a root exists removes the
  root. The `std::exit` control leaves it by the same route.

### clang-tidy on the header, on macOS

This was seen later, while [[BPR-1127]] ran its gates. That card's two
sources include `tests/support/test_expectation.hpp`. `tidy.py --paths` on
them reports one finding in the header, with clang-tidy 22.1.8 (`llvm@22`)
and with 23.1.1:
`test_expectation.hpp:137:24: bugprone-throwing-static-initialization`, for
`inline failure_state State;`. Version 23 also reports
`bugprone-std-exception-baseclass` at line 152, the `throw
expectation_failed{ }`, which the header does on purpose. The Linux run
found none in the header. A likely reason is that the member initializer
calls `::getpid()`. The macOS SDK declares it without `noexcept`
(`unistd.h:472`), so the default constructor may throw. glibc marks it
`__THROW`. This is recorded for AC7 on this card's claim. Nothing was
changed here.

### The header's static state, fixed at `053abefe` on Linux — bpr-main-linux, 2026-09-24

The coordinator asked for this fix, for AC7. The commit, `053abefe`, pushed
without a rebase, changes only `tests/support/test_expectation.hpp` and this
card.

- **The change.** `detail::process_id()` is a `noexcept` wrapper for
  `::getpid()`, compiled only where there is one. `State`'s initializer and
  `may_unwind` both call it. The state's default constructor is declared
  `noexcept`. Behaviour is unchanged: `getpid` cannot fail.
- **The deliberate throw.** `throw expectation_failed{ }` is left as it is.
  It now carries `NOLINTNEXTLINE(bugprone-std-exception-baseclass)` and a
  line that says why. The type is not a `std::exception` on purpose (see
  `expectation_failed`).

**Reproduced on Linux first.** clang-tidy 21 has both checks under their
older names, `cert-err58-cpp` and `hicpp-exception-baseclass`. glibc
declares `getpid` `noexcept`, which is why the Linux run never saw the
finding. A probe translation unit, not committed, declares a stand-in for
`getpid` without `noexcept`, as the macOS SDK does. It then includes the
header, with the repository's exclusion of `build/` headers turned off.

| Header | `getpid` without `noexcept` | glibc's `getpid` |
| --- | --- | --- |
| before (`1a2cdc9d…c6c25`) | `137:24 cert-err58-cpp` for `inline failure_state State;`, and `152:9 hicpp-exception-baseclass`: the Mac's two findings | none |
| after | no initializer finding. The throw is still reported under the old name, which the `NOLINT` does not name. With the old name put in the `NOLINT` of a copy, there are 0 findings | none |

The compiler agrees. With that `getpid`,
`static_assert(noexcept(bpr::tests::detail::failure_state{ }))` fails on
the old header and holds on the new one.

**Checks on Linux.**

- `tidy.py` with clang-tidy 21 on `profile_runtime_lease_tests.cpp` and
  `quill_logging_tests.cpp` passes, with nothing in the header.
- `/usr/bin/clang-format` 22 `--dry-run -Werror` on the header passes.
- The whole `build/dev` graph rebuilt 46 translation units, with no
  `warning:` line.
- All 56 Linux rows of the class pass, serially and offscreen, with fresh
  `HOME` and XDG roots. They print the same lines as
  `candidate-final-32af9f3f-verbose.log`: 56 of 56 match after that log's
  own normalization. No `/tmp` entry was added, and none in
  `/run/user/1000`.

The probe, its outputs and the run logs are in
`build/logs/bpr-1128/bpr1121-tidy/` and `build/logs/bpr-1128/bpr1121-*`,
and are not committed.

**Owed on the Mac.** Run tidy again on the header, through the two sources
that found it, with both versions:

```sh
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --clang-tidy "$(brew --prefix llvm@22)/bin/clang-tidy" --paths tests/platform/macos_capsule_identity_tests.cpp tests/platform/macos_supervisor_identity_continuity_tests.cpp
```

Nothing in `test_expectation.hpp` should be reported, with 22.1.8 or with
23.1.1.

### AC7 on macOS: tidy finds nothing in the header (bpr-main-mac, 2026-09-25T05:19Z)

This is the Mac run owed above, at `77e9e776`. The header there is
`053abefe`'s (SHA-256 `ef4ccf23…b0ce3`). Neither of the two sources has
changed since `5f527e2c`. `build/dev`'s compilation database was current.
The card stays on bpr-main-linux's claim.

| clang-tidy | The command above, on the two sources | Findings |
| --- | --- | --- |
| 22.1.8 (`llvm@22`) | `clang-tidy passed.`, exit 0 | none. One warning suppressed, in non-user code |
| 23.1.1 (`llvm`) | `clang-tidy passed.`, exit 0 | none. Suppressed: non-user code, and one `NOLINT`, the deliberate throw's |

**Negative control.** The header was not edited in the shared tree for this.
Instead, a scratch copy was made of `macos_capsule_identity_tests.cpp` and
`tests/support/`, holding the header as it was before `053abefe` (SHA-256
`1a2cdc9d…c6c25`, the Mac's original). clang-tidy ran on the copy with
`build/dev`'s compile command, only the file path changed, and with the
extra arguments `tidy.py` passes.

| Header in the copy | 22.1.8 | 23.1.1 |
| --- | --- | --- |
| before `053abefe` | `test_expectation.hpp:137:24` `bugprone-throwing-static-initialization` | the same, and `152:9` `bugprone-std-exception-baseclass`: the Mac's two findings |
| at `053abefe` | none | none; one `NOLINT` suppressed |

So the header is inside the checks' scope on this station. Under both
versions the fix removes the initializer finding, and the `NOLINT`
suppresses the throw's. AC7's tidy clause holds on macOS for the header and
these two sources. The scratch copy was removed afterwards. The logs are
station records in `build/logs/bpr-1121-mac-ac7/`, not committed.

## Windows, AC2 and AC6 (bpr-main-win, 2026-09-29)

Windows 11 Home 10.0.26200, MSVC 19.51, Qt 6.11.2, `build/windows-dev`
Debug with warnings as errors, at `22d18416` with the build fixes of
`537f718d`. None of those fixes touches `test_expectation.hpp` or this
card's handling of a failure. This entry does not move the card, which
stays on bpr-main-linux's claim.

### The build and the Windows-only source

The whole graph builds with no `warning` line.
`tests/end_to_end/windows_profile_storage_startup_contract_tests.cpp`, the
source built only here, compiles, and its row
`application_windows_profile_storage_startup_contract` passes.

### The rows

The card's selection, with the Windows-only row, matches 86 rows here. They
ran serially in 1,052 s. **77 pass.** Nine fail, and none fails at the
unwinding this card added:

| Row | Failure | Why it is not this card's |
| --- | --- | --- |
| `qt_markdown_body_1_0`, `_1_25`, `_2_0` | contained: `QImage::save: … Cannot open device for writing: Access is denied`, then `FAILED: write designated canary image`, and `QTemporaryDir: Unable to remove` | The resource `QTemporaryDir` is made inside the AppContainer with a protected DACL: this user full control, and `Everyone` and the low label only read control and attributes. It has no ACE for the container, so the contained process can write neither in it nor remove it. Run directly, the same executable passes. Not bisected |
| `sqlite_review_tour_storage` | contained, exit 3 after `each command client edit advances shared history` | a new RFC-BPR-0045 store row. It passes run directly and is made uncontained in `532cffaf` |
| `qt_agent_launch_canary` | contained: `open durable child course` | an RFC-BPR-0023 course; see [[BPR-1139]]'s Windows entry |
| `live_review_submission_restart_recovery` | `--kill-prepared exited before checkpoint`: `FAILED: submission store opens` | a store open, not a failure ending |
| `application_profile_activation_public_path` | `profile_activation_contract.py: error: unrecognized arguments` | the row passes an argument its script no longer accepts |
| `application_profile_owner_admission_public_path` | `SEED_REGISTRY_REFUSAL:0`, `FAILED: seed registry` | the seed step refuses the registry under the container's temporary directory |
| `application_profile_window_review_restoration_public_path` | `FAILED: failed reading restoration preserves exact recorded identities and sibling` | a restoration expectation, reported through this card's `FAILED:` line as intended |

`application_device_binding_restart`, the macOS trial's second row, passes
here (40.46 s).

### A forced failure while a root exists

The trial hook is Linux's, as the macOS entry describes, rebuilt here and
never committed. `BPR_TRIAL_FAIL_MESSAGE` fails the first expectation
whose message contains the text. `BPR_TRIAL_NO_UNWIND` makes `fail()` take
its `std::exit` branch, the negative control. The header's SHA-256 was
`ef4ccf23…b0ce3` before, `c6d78168…7e49f` with the hook, and `ef4ccf23…b0ce3`
after the restore. `git status` was then clean. Only
`test_windows_profile_storage_startup_contract` was built with the hook, and
it was rebuilt after the restore. Its image contains no `BPR_TRIAL`, and it
passes.

The executable ran directly, as its row does, since it is uncontained. Its
root is `bpr-0806-<clock>` in `%TEMP%`, which was listed before and after
each run.

| Expectation failed | Root at that point | Unwinding (as committed) | `std::exit` (control) |
| --- | --- | --- | --- |
| none | | exit 0, `windows-profile-storage-startup-contract:pass`, nothing left | |
| `the registry file exists` | the owner-private root, holding `state\v1\profile-registry.sqlite3` | exit 1, the `FAILED:` line, nothing left | exit 1, the same line, and it left the root with `state\v1\profile-registry.sqlite3` |
| `scratch root is owner-private`, inside the root's constructor | the root just made, held by its `scratch_guard` | exit 1, the `FAILED:` line, nothing left | exit 1, and it left the empty root |

A first attempt of the second case ran under a PowerShell driver that
stopped its pipeline on the child's first line of standard error. It left a
root. That is the driver ending the child, not the course. The corrected
driver's run is the one in the table. All three trial roots were removed by
exact name afterwards.

### Acceptance state after this run

- **AC6 on Windows: met for this card's change, with nine rows red for
  causes outside it.** The Windows-only source compiles and its row passes,
  and 77 of the 86 rows pass. No failure is in `test_expectation.hpp` or
  in how a failure ends.
- **AC2, repeated on Windows:** a failure while a root exists removes the
  root, including one inside a root's constructor. The `std::exit` control
  leaves it.

The logs, the hook and the trial driver are station records in
`C:\Users\mikem\bpr-main-helpers\run\station-0929\` (`1121.log` and `.xml`,
`trial-1121.txt`) and `…\station-0929\trial\`, and are not committed.

## Finished on bpr-main-mac (2026-10-01)

bpr-main-linux's claim was taken over by the Mac integrator, on the
coordinator's direction, to judge the card against its acceptance now that
all three stations have reported. No source changed in this step.

### The integration pass at `4aa6f544d`

macOS 26 on Apple Silicon, Apple clang, Qt 6.11.2, `build/dev` Debug. The
build was current (`ninja: no work to do`, no `warning:` line). The card's
selection, serially, with the native menu-bar row left out, ran from
15:24:56Z to 16:04:10Z, beside [[BPR-1139]]'s rows:

| Rows | Result |
| --- | --- |
| The card's selection on macOS: 92 rows | **91 pass**, among them `gh_cli_git_smart_transport`, `release_update_transaction`, `application_device_binding_restart` and the three `qt_markdown_body_` rows |
| `application_profile_window_review_restoration_public_path` | fails at `FAILED: failed reading restoration preserves exact recorded identities and sibling`, after 454.50 s. This is [[BPR-1236]]'s failure, which that card records at the same check on macOS, Windows and Linux. The line is this card's `FAILED:` ending, as intended |
| `/private/tmp`, listed before and after | no `bpr-` entry added |

### The Windows reds, by the card that owns each

The Windows run of 2026-09-29 failed nine rows, none at this card's
unwinding. Each now has its own owner:

| Row | Owner | State |
| --- | --- | --- |
| `qt_markdown_body_1_0`, `_1_25`, `_2_0` | [[BPR-1232]] | Done at `491c682f`: pass contained on Windows |
| `sqlite_review_tour_storage` | `532cffaf` | runs uncontained on Windows |
| `live_review_submission_restart_recovery` | [[BPR-1233]] | Done |
| `qt_agent_launch_canary` | [[BPR-1229]] | Backlog, the owner's containment decision |
| `application_profile_activation_public_path` | [[BPR-1234]] | Backlog, Windows |
| `application_profile_owner_admission_public_path` | [[BPR-1235]] | Backlog, Windows |
| `application_profile_window_review_restoration_public_path` | [[BPR-1236]] | Backlog; fails on every platform |

### The judgment

- **AC6 is met for this card's change.** The four sources built only on
  macOS or Windows compile there with no warning, and the Windows-only row
  passes. On each platform every row of the class passes except rows whose
  failure is recorded, with its cause, on the card that owns it above. No
  failure is in `test_expectation.hpp` or in how a failure ends, and on
  macOS and Windows a forced failure while a root exists removes the root
  while the `std::exit` control leaves it.
- `application_profile_empty_desktop_native_menu_bar` compiles on macOS.
  Its run needs the owner's unlocked Aqua session, and the row's own state
  is [[BPR-1250]]'s, parked for the owner. It is not run here.
- AC1 to AC5 and AC7 stand on the Linux, macOS and Windows entries above.

The logs are station records in `build/logs/bpr-1121-1128-1139-takeover/`
(`integration.log`, `rows.txt`, `tmp-before.txt`, `tmp-after.txt`), not
committed.
