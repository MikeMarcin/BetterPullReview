---
schema: "implement-rfc.card.v1"
id: "BPR-1044"
title: "Twelve courses root their data directly in the system temporary directory, so BPR-0817's workspace recovery refuses at every startup"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-1042's Linux run","found_at":"e54d203e","surveyed_at":"7eb4aa87","related_cards":["BPR-0817","BPR-0856","BPR-0874","BPR-0875","BPR-1027","BPR-1042"],"run":"bpr-main-linux-20260923","reservation":"docs/issues/_reservations/BPR-1044--BPR-1044--5fbefa42571d.json"}
---

# BPR-1044: Twelve courses root their data directly in the system temporary directory, so BPR-0817's workspace recovery refuses at every startup

## Defect

Twelve composed courses print this line at every application startup, and
still pass:

```text
AGENT_WORKSPACE_RECOVERY_REFUSED:/tmp/<course>-XXXXXX/agent-workspace-ownership-v1:repair-required
```

Each of them uses a bare `QTemporaryDir` as its application data root. The
root itself is owner-private: every course narrows it with
`make_storage_root_owner_private`. The problem is the root's **parent**, which
is the shared temporary directory. On the Linux station that is `/tmp`, mode
`1777`, owned by root, on tmpfs, with `TMPDIR` unset.

The application puts its agent workspace neighbourhood beside the data root,
as `<root-leaf>-agent-workspaces-v1` (`src/app/src/github_application.cpp`,
`agentWorkspaceLeaf`). The neighbourhood's parent is therefore `/tmp`.
`prepare_workspace_neighbourhood` accepts a parent only if this user owns it
and no group or other can write it (`workspace_parent_trusted`,
`src/adapters/git/src/installed_git_process.cpp`). `/tmp` fails both checks.
Startup recovery therefore returns `repair_required`, and [[BPR-0817]]'s
decision applies: the refusal is printed once, every window states it in its
status bar, and disposable agent workspaces are unavailable for the run.

The product is right to refuse. The courses run in a state that a real
install reaches only when its data folder needs repair.

The reserved title said the courses create their roots "without
owner-private permissions". The survey below shows the roots are
owner-private. What the twelve courses lack is an owned level above the
root.

## Survey (bpr-main-linux, 2026-09-23)

The survey ran on JaegAW: WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2, `build/dev`
(Debug) built at `7eb4aa87`. It was serial and offscreen: `ctest -j1 -V` with
`QT_QPA_PLATFORM=offscreen`, `DISPLAY` and `WAYLAND_DISPLAY` unset, and fresh
XDG data, config, cache, state and runtime directories. It selected all 164
rows that are `application_*` or whose command runs an end-to-end binary.
161 passed. The three failures are unrelated to this card:

- `application_uiforetw_selected_diff_contract` is [[BPR-1036]].
- `application_agent_finding_dif_context_contract` is [[BPR-0966]]'s known
  failure.
- `application_threaded_reply_contract` fails at `THREADED_REPLY_FAILED`,
  phase 1. It failed the same way on a rerun on a quiet station.

The sweep took 21 min 27 s. The station records are
`build/logs/bpr-main-linux-20260923/bpr-1044-survey-ctest.log` and
`bpr-1044-rerun-two-failures.log`, and are not committed.

**13 rows printed the line, 33 times in all, always `repair-required`.** One
of those rows is deliberate:

| Row | Lines | Data root |
| --- | ---: | --- |
| `application_device_binding_launch_renewal` | 8 | bare `QTemporaryDir` |
| `application_device_binding_restart` | 7 | bare `QTemporaryDir` |
| `application_setup_destination_public_path` | 3 | bare `QTemporaryDir`, in each course |
| `application_review_submission_composition_public_path` | 3 | bare `QTemporaryDir` (`data.path()`) |
| `application_attended_recent_replay` | 2 | bare `QTemporaryDir`, in both courses |
| `application_profile_startup_public_path` | 2 | bare `QTemporaryDir` |
| `application_selected_file_safety_public_path` | 2 | the setup-destination executable, so the same roots |
| `application_logging` | 1 | bare `QTemporaryDir` |
| `application_personal_discovery_public_path` | 1 | bare `QTemporaryDir` |
| `application_profile_multi_window_public_path` | 1 | bare `QTemporaryDir` |
| `application_profile_two_account_isolation_public_path` | 1 | bare `QTemporaryDir` |
| `application_release_update_public_path` | 1 | bare `QTemporaryDir` (`root(temporary)`) |
| `application_profile_management_public_path` | 1 | **deliberate**: [[BPR-0817]]'s pre-existing `0775` `shared-parent` section, which asserts this refusal |

So **12 rows print 32 lines that nobody intended.** Every one of them is a C++
course that sets `ApplicationDataRoot` to its `QTemporaryDir` path.

**The cause is in each course, not in a shared helper:**

- No row that uses the shared Python helper
  (`scripts/contained_temporary_support.py`,
  `contained_temporary_directory`) printed the line.
- No CMake course that uses [[BPR-0875]]'s `bpr_private_data_root` printed it.
- No C++ course that already keeps its root one owned level down printed it.
  These are the agent chat, finding and invocation courses,
  `github_application_liveness_tests.cpp` (`application_poller_liveness`),
  the large-review courses ([[BPR-0856]], [[BPR-0874]]),
  `profile_management`'s main root, `profile_second_window_close`
  ([[BPR-1027]]) and `release_maintenance`.

**Platforms.**

- **Linux**: measured here.
- **Windows**: reported as well. [[BPR-1042]]'s Windows evidence says every
  run of that course prints the line, and that "Windows courses whose data
  root sits directly in a `QTemporaryDir` print it, including ones that
  pass". [[BPR-1027]]'s "Offscreen" section says that without the owned level
  the agent host is left out of the composition. That claim is Windows
  evidence and was not measured here.
- **macOS**: not expected. The runner's `TMPDIR` is a per-user private
  directory, which `workspace_parent_trusted` accepts ([[BPR-0856]]'s
  comment in the large-review benchmark).

## Scope

- Give each of the twelve courses' data roots one owned level above them:
  an owner-private temporary parent with a `data` child. This is the
  arrangement [[BPR-0874]] made uniform for the large-review courses and
  [[BPR-1027]] used for the second-window course. Keep the root spelled
  through `native_filesystem_path` so Windows admits it.
- A small shared C++ test helper is the natural home. There are already
  eight hand-written copies of this arrangement. [[BPR-0875]]'s
  `bpr_private_data_root` is the CMake precedent.
- **Windows containment.** Narrowing the temporary parent needs `WRITE_DAC`,
  which an AppContainer lacks ([[BPR-0833]]). [[BPR-0874]] had to pin its
  rows `WINDOWS_UNCONTAINED` for that reason. Several of the twelve rows are
  already pinned (`scripts/verify_test_network_isolation.py`). For any that
  are contained, measure before pinning, or use
  `create_storage_root_owner_private` with the container's entry.
- Leave `application_profile_management_public_path`'s `shared-parent`
  section alone. It is the course that proves the refusal.

## Acceptance

- AC1 On Linux, a serial offscreen sweep of the same rows prints
  `AGENT_WORKSPACE_RECOVERY_REFUSED` only from
  `application_profile_management_public_path`'s deliberate `shared-parent`
  section, and each of the twelve rows still passes.
- AC2 On Windows, the rows registered there no longer print the line, and
  each passes as it is registered, contained or uncontained. Any new pin
  records the cause that was measured.
- AC3 `application_profile_management_public_path` still asserts the
  deliberate refusal and passes on Linux and Windows.

## One helper gives the twelve courses their owned level (bpr-main-linux, 2026-09-23)

Station `bpr-main-linux` (JaegAW: WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2,
`build/dev` Debug from the `dev` preset, which compiles with `-Werror`),
claimed at `7cd4ab5d`. The change landed as `3307ccf2`. It was verified on
`7cd4ab5d` plus the change, and the commits rebased under it touch only
cards. Every run below was serial and offscreen: `ctest -j1
-V`, `QT_QPA_PLATFORM=offscreen`, `DISPLAY` and `WAYLAND_DISPLAY` unset, and a
fresh `HOME` and fresh XDG data, config, cache, state and runtime
directories, removed afterwards. `TMPDIR` was unset, so every `QTemporaryDir`
was made in `/tmp`. The logs are under
`build/logs/bpr-main-linux-20260923/bpr-1044/`. They are station records and
are not committed.

### Re-check at HEAD

At `7cd4ab5d` the survey's 13 rows all passed, and they printed the line 33
times, exactly as the survey counted: 7, 8, 3, 3, 2, 2, 2, 1, 1, 1, 1 and 1
from the twelve rows, and 1 from `application_profile_management_public_path`'s
`shared-parent` section (`before-7cd4ab5d-rows13.log`).

### The change

**The helper.** `tests/end_to_end/private_data_root.hpp` holds
`bpr::e2e::private_data_root(const QTemporaryDir&)`. It narrows the
temporary directory with `make_storage_root_owner_private`, creates
`<temporary>/data` with `create_storage_root_owner_private`, and returns that
root, canonical and spelled through `native_filesystem_path`. If the
directory is invalid or either step is refused, it returns nothing. This is
the arrangement [[BPR-0874]] and [[BPR-1027]] use. The helper is header-only
and sits beside `public_path_wait.hpp`. Every course already links the
SQLite adapter and the application headers through `BetterPullReview::Composition`,
so no CMake change was needed.

**The courses.** There are 19 call sites in 11 files:

| File | Rows | Sites |
| --- | --- | ---: |
| `device_binding_launch_renewal_tests.cpp` | `application_device_binding_launch_renewal` | 1 |
| `device_binding_restart_tests.cpp` | `application_device_binding_restart` | 1 |
| `setup_destination_public_path_tests.cpp` | `application_setup_destination_public_path`, `application_selected_file_safety_public_path` (the same executable with `--selected-file-safety`) | 8 |
| `review_submission_composition_public_path.cpp` | `application_review_submission_composition_public_path` | 1 |
| `attended_recent_replay_tests.cpp` | `application_attended_recent_replay` | 2 |
| `profile_startup_public_path_tests.cpp` | `application_profile_startup_public_path` | 1 |
| `application_logging_tests.cpp` | `application_logging` | 1 |
| `personal_discovery_public_path_tests.cpp` | `application_personal_discovery_public_path` | 1 |
| `profile_multi_window_public_path_tests.cpp` | `application_profile_multi_window_public_path` | 1 |
| `profile_two_account_isolation_public_path_tests.cpp` | `application_profile_two_account_isolation_public_path` | 1 |
| `release_update_public_path_tests.cpp` | `application_release_update_public_path` | 1 (its `root()`) |

- Each site keeps its existing `expect` message, and a refusal still stops
  the course at the same step.
- The review submission course was the one course that did not narrow its
  root, and it used Qt's spelling (`data.path().toStdString()`). It now uses
  the helper and prints `review-submission-composition:fail private data root`
  if the helper refuses.
- `release_update_public_path`'s `root()` also serves two courses that open
  update stores without starting the application. Those stores now sit in a
  `data` child too, which changes nothing they check.
- Includes the courses no longer use were removed.
- The application now creates the workspace neighbourhood
  (`data-agent-workspaces-v1`) inside the temporary directory, so it is
  removed with it. No run left anything in `/tmp`.

**Left alone.** `application_profile_management_public_path`'s
`shared-parent` section is unchanged. So are the eight courses that already
had an owned level, which the survey above lists. They did not print the
line. Moving them onto the helper would change rows outside this card's
acceptance, and several of them have Windows arrangements that were measured
separately ([[BPR-0874]], [[BPR-1027]]).

**No pinned code.** Nothing under `src/` changed, so the round-trip
contract's pins are untouched and no reseal is needed.

**Windows registration.** All twelve rows are already `WINDOWS_UNCONTAINED`
and pinned `dos-volume-name` in `scripts/verify_test_network_isolation.py`.
So narrowing the temporary directory, which needs `WRITE_DAC`, does not meet
a container. This card adds no pin and changes no registration.

### Verification (Linux)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | Exit 0, no `warning:` line. It relinked the 11 course executables and nothing else |
| The 13 rows at the candidate (`after-candidate-1-rows13.log`) | 13/13 passed, in 1.77-28.97 s each. **One** `AGENT_WORKSPACE_RECOVERY_REFUSED` line, from `application_profile_management_public_path`: `/tmp/profile_management_public_path-ClFSvs/shared-parent/BetterPullReview/agent-workspace-ownership-v1:repair-required`. The section still asserts the refusal, and the row passes |
| Negative control (`negctl-rows13.log`) | The helper returned the narrowed temporary directory itself as the root, with no `data` child. All 33 lines came back, with the same count in each row. So the owned level, not the narrowing, is what removes them. The edit was then undone. The helper's SHA-256 is back to the candidate's (`d43c28be…e824`), and `grep -rc BPR1044_NEGCTL src tests` finds nothing |
| AC1's sweep: the survey's 164 rows at the candidate (`sweep-candidate.log`) | 162 of 164 passed, in 1,216.86 s. **One** `AGENT_WORKSPACE_RECOVERY_REFUSED` line, from `application_profile_management_public_path`'s `shared-parent` section. The twelve rows pass (1.84-29.00 s). The two failures are the known ones: `application_agent_finding_dif_context_contract` ([[BPR-0966]], `dif-context-evidence failed=dif-comment-ready-after-conversion dif-acknowledge-after-acceptance`) and `application_uiforetw_selected_diff_contract` ([[BPR-1036]], `UIFORETW_CAPTURE_LAYOUT_FAILED:987:842 width_floor=750 height_floor=850`). `application_threaded_reply_contract`, the survey's third failure, now passes (2.82 s, [[BPR-1046]]) |
| `core_boundary_contract`, `architecture_boundaries`, `architecture_boundary_contract`, `conditional_branch_identifiers`, `conditional_branch_identifier_contract`, `documentation_structure`, `test_network_isolation_manifest` | 7/7 passed |
| `/usr/bin/clang-format` 22.1.8 on the 12 files | Clean |
| `_coding-standard/scripts/tidy.py` on the 11 translation units (clang-tidy 21.1.8) | No finding on a changed line or in the helper. The two it reports are older than this card: `dif_runtime.hpp:1` and `std::endl` at `device_binding_launch_renewal_tests.cpp:78` |

### Acceptance state

- **AC1** (on Linux, a serial offscreen sweep of the same rows prints the
  line only from `profile_management`'s `shared-parent` section, and each of
  the twelve rows passes): **pass.** The sweep printed the line once, from
  that section, and each of the twelve rows passed. The negative control
  shows that the check can fail.
- **AC2** (on Windows, the registered rows no longer print the line and pass
  as registered; any new pin records its measured cause): **owed by
  bpr-main-win.** No pin was added.
- **AC3** (`profile_management` still asserts the deliberate refusal and
  passes on Linux and Windows): **Linux passes.** On Windows the
  `shared-parent` section is compiled out (`#if !defined(_WIN32)`), because
  it depends on POSIX modes. So Windows can show only that the row passes.
  **That run is owed by bpr-main-win.**

### Still owed: the Windows run (bpr-main-win)

On a head that contains `3307ccf2`, rebuilt:

```powershell
cmake --build --preset windows-dev
ctest --preset windows-dev -j1 -V -R "^(application_device_binding_launch_renewal|application_device_binding_restart|application_setup_destination_public_path|application_review_submission_composition_public_path|application_attended_recent_replay|application_profile_startup_public_path|application_selected_file_safety_public_path|application_logging|application_personal_discovery_public_path|application_profile_multi_window_public_path|application_profile_two_account_isolation_public_path|application_release_update_public_path|application_profile_management_public_path)$"
```

Expect:

- All 13 rows pass as registered. All of them are uncontained.
- None of the twelve prints `AGENT_WORKSPACE_RECOVERY_REFUSED`. Record any
  line with its row and path.
- `application_profile_management_public_path` passes. Its `shared-parent`
  section does not run on Windows. Its main root is `<QTemporaryDir>/data`,
  and that temporary directory is not narrowed. If the row prints the line
  from its main root, record it. It would be the same cause, in a course this
  card left alone, and it would need a follow-up card.

When that run is recorded, the card can finish.

## Windows: 13 of 13 pass, and none prints the line (bpr-main-win, 2026-09-23)

The run used `8595684c`, which contains `3307ccf2`, with a clean tree.
`build/windows-dev` (Visual Studio generator, MSVC 19.51, Debug, Qt 6.11.2)
was rebuilt first, and the build exited 0 with no `error` or `warning` line.
Then the card's selector ran:
`ctest --preset windows-dev -j1 -V -R "^(application_device_binding_launch_renewal|…|application_profile_management_public_path)$"`.
The logs are station records under `build/logs/bpr-1044-main-win/` and are
not committed.

| Row | Result |
| --- | --- |
| `application_device_binding_restart` | Passed, 38.37 s |
| `application_device_binding_launch_renewal` | Passed, 31.03 s |
| `application_logging` | Passed, 7.34 s |
| `application_review_submission_composition_public_path` | Passed, 9.69 s |
| `application_setup_destination_public_path` | Passed, 14.07 s |
| `application_selected_file_safety_public_path` | Passed, 19.94 s |
| `application_attended_recent_replay` | Passed, 25.73 s |
| `application_profile_startup_public_path` | Passed, 9.41 s |
| `application_profile_management_public_path` | Passed, 14.18 s |
| `application_release_update_public_path` | Passed, 7.22 s |
| `application_profile_multi_window_public_path` | Passed, 48.23 s |
| `application_personal_discovery_public_path` | Passed, 10.58 s |
| `application_profile_two_account_isolation_public_path` | Passed, 20.76 s |

- **All 13 pass as registered**, all uncontained
  (`bpr_windows_uncontained = 256.54 sec*proc (13 tests)`). No row was
  re-registered and no pin was added.
- **`AGENT_WORKSPACE_RECOVERY_REFUSED` appears 0 times** in the whole verbose
  log, so none of the twelve prints it. `application_profile_management_public_path`
  prints nothing from its main root either. Its `shared-parent` section is
  compiled out on Windows, so the row can only show that it passes, and it
  does.
- The only epoch lines are `application_logging`'s deliberate
  `EPOCH_CACHE_FAILURE:2:` and `EPOCH_EFFECT_FAILED:revision-prepare:0`, which
  that course provokes and passes on.

**AC2 on Windows: pass. AC3 on Windows: the row passes; the deliberate
section cannot run there.** The card's owner decides whether it can finish.

**The same line in a course this card left alone.** While measuring
[[BPR-1040]], this station saw
`AGENT_WORKSPACE_RECOVERY_REFUSED:…\Temp\bpr-0709-restart-<id>\<phase>\agent-workspace-ownership-v1:repair-required`
from `application_profile_window_review_restoration_public_path`. That course
is Python-driven. Each data root is a child of the course's own temporary
directory, which its `os.chmod(root, 0o700)` does not narrow on Windows. The
line appeared in the output of runs that failed for another reason, and the
course prints the application's stderr only when a phase fails, so whether
passing runs print it too is not known. It looks like this card's cause in a
course outside the twelve. It is recorded here for whoever takes the
remaining courses. The gate for bare temporary roots that BPR-1050 is
reserved for may or may not reach a Python course; that is for its owner to
say.

## Every criterion has its evidence; the rows again at main (bpr-main-linux, 2026-09-23)

**The Windows record meets AC2 and AC3's Windows half.** `8595684c`
contains `3307ccf2`. All 13 rows pass as registered, and all 13 are
uncontained. The line appears 0 times, and no pin or registration was added.
[[BPR-0817]]'s `14a18b45` compiled the `shared-parent` section out on Windows
with `#if !defined(_WIN32)`, which is older than this card. This card does not
touch that course. So on Windows the deliberate assertion never ran, and
AC3's Windows half can only be that the row passes, which it does. On Linux
the section runs and asserts the refusal.

**The same 13 rows at main's `b60e04b0` on Linux.** [[BPR-1045]]'s
`e02a001e` later added a section to `profile_management_public_path_tests.cpp`,
before the `shared-parent` section. It did not change that section. Main has
also changed the profile desktop, the application host and the workbench
drawer since `3307ccf2`. So the rows were run again on this station's own
`build/dev`: GCC 15.2, Qt 6.11.2, Debug, `-Werror`, current, and `ninja` had
no work to do. They ran serially and offscreen, with `ctest -j1 -V`,
`QT_QPA_PLATFORM=offscreen`, `DISPLAY` and `WAYLAND_DISPLAY` unset, and a
fresh `HOME` and fresh XDG data, config, cache, state and runtime
directories, which were removed afterwards. 13 of 13 passed, exit 0, in
2.43 to 28.96 s. The log has **one** `AGENT_WORKSPACE_RECOVERY_REFUSED`
line, from the deliberate section:
`/tmp/profile_management_public_path-widEwM/shared-parent/BetterPullReview/agent-workspace-ownership-v1:repair-required`.
Nothing was left in `/tmp`. The log is a station record,
`build/logs/bpr-main-linux-20260924/impl/bpr-1044-rows13-b60e04b0.log`, and
is not committed.

**Acceptance.**

- **AC1: pass on Linux at `3307ccf2`.** In the 164-row sweep, the line came
  only from the `shared-parent` section, and the twelve rows passed. The
  negative control brought all 33 lines back. The same holds at `b60e04b0`
  for the 13 rows.
- **AC2: pass on Windows at `8595684c`.** 13 of 13 passed uncontained as
  registered, the line appeared 0 times, and there is no new pin.
- **AC3: pass.** The deliberate section asserts the refusal on Linux, and
  the row passes (`3307ccf2`, `b60e04b0`). On Windows the row passes
  (`8595684c`), and the section has been compiled out there since
  [[BPR-0817]].

The Python-driven course recorded above, which prints the line on Windows
from a data root the course does not narrow, is outside the twelve.
[[BPR-1050]], now Ready, records it in its scope. It does not hold this card
open.

The card is finished with these records.
