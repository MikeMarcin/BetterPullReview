---
schema: "implement-rfc.card.v1"
id: "BPR-1050"
title: "Move the eight courses that hand-write the owner-private data-root arrangement onto bpr::e2e::private_data_root, and fail a gate when a course roots data in a bare temporary directory"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-1044","found_at":"3307ccf2","surveyed_at":"d4f9d07d","related_cards":["BPR-1044","BPR-0817","BPR-0874","BPR-0875","BPR-1027","BPR-0812","BPR-0832","BPR-1031","BPR-0833"],"reservation":"docs/issues/_reservations/BPR-1050--BPR-1051--e733678668d2.json"}
---

# BPR-1050: Move the eight courses that hand-write the owner-private data-root arrangement onto bpr::e2e::private_data_root, and fail a gate when a course roots data in a bare temporary directory

## Why

[[BPR-1044]] found twelve composed courses that used a bare `QTemporaryDir`
as their application data root. The application puts its agent workspace
neighbourhood beside the root, so the neighbourhood's parent was the shared
temporary directory. On Linux that is `/tmp`, mode `1777`, owned by root.
[[BPR-0817]]'s startup recovery correctly refused it, and every startup
printed `AGENT_WORKSPACE_RECOVERY_REFUSED:…:repair-required`. On Windows the
same thing happened under `%TEMP%`.

BPR-1044 fixed those twelve with one header-only helper,
`bpr::e2e::private_data_root(const QTemporaryDir&)` in
`tests/end_to_end/private_data_root.hpp` (`3307ccf2`). It narrows the
temporary directory to owner-private, creates a `data` child under the
storage policy, and returns that child, canonical and spelled through
`native_filesystem_path`.

Two things were left, and this card takes both.

1. **Eight courses still hand-write the same arrangement.** BPR-1044 left them
   alone on purpose. They did not print the line, and moving them would have
   changed rows outside its acceptance. Their copies differ in small ways that
   the helper already settles once.
2. **Nothing stops the next course.** A new course that roots its data in a
   bare temporary directory passes. It prints the refusal line on Linux and
   Windows, but no row fails on it. Before BPR-1044's survey, twelve rows
   printed it on every run and still passed.

## The eight copies (read at `d4f9d07d`)

| File | Rows | The copy | How it differs from the helper |
| --- | --- | --- | --- |
| `agent_chat_composition_public_path.cpp:255-262` | `application_agent_chat_{composition,stale,continued,anchored_continuation,construction_cut}_contract` | narrows the parent, `create_directory` of `data`, narrows `data` | Spells the root from Qt's `data.path().toStdString()`: not canonical, not native |
| `agent_finding_composition_public_path.cpp:283-290` | the nine `application_agent_finding_*_contract` composition rows, including `dif_context` ([[BPR-0966]]'s known failure) | the same as the chat course | The same spelling |
| `agent_invocation_composition_public_path.cpp:558-569` | the eight `application_agent_invocation_*_contract` rows | `create_directories` of `data`, narrows `data` only | Does not narrow the parent. Only the `QTemporaryDir` branch moves. The crash-materializing and external-root branches take the root from `argv` (`application_agent_crash_recovery`, `application_agent_materialization_crash_recovery`) and stay as they are |
| `github_application_liveness_tests.cpp:56-62` | `application_poller_liveness` | narrows the parent, creates and narrows `data` | Spells the root from `canonicalDataRoot.toStdU16String()`, not through `native_filesystem_path` |
| `large_review_benchmark.cpp:890-897` | `application_large_review_benchmark`, `application_unavailable_diff_public_path`, `application_approximate_diff_public_path` | narrows the parent, creates and narrows `data` | Native but not canonical |
| `profile_management_public_path_tests.cpp:115-123`, the main root only | `application_profile_management_public_path` | `create_storage_root_owner_private` of `<canonical>/data` | Does not narrow the parent |
| `profile_second_window_close_public_path_tests.cpp:167-176` | `application_profile_second_window_close_public_path` | narrows the parent, `create_storage_root_owner_private` of `data` | None. It is the helper written out |
| `release_maintenance_public_path_tests.cpp:104-108` | `application_release_maintenance_public_path` | `create_storage_root_owner_private` of `<canonical>/data` | Does not narrow the parent. Its `QTemporaryDir` sits under `QUALIFICATION_TARGET_ROOT` when that is set, which the helper accepts as it is |

`profile_management_public_path_tests.cpp`'s `shared-parent` section
(`:455-501`) is **not** a copy. It builds a `0775` parent on purpose to assert
the refusal, and it stays as it is. It creates that parent beside the main
root, inside the temporary directory, so narrowing the temporary directory
does not reach it.

### Windows registration

Every row above is in `WINDOWS_UNCONTAINED_TESTS`, pinned
`dos-volume-name` (`scripts/verify_test_network_isolation.py`). Narrowing the
temporary directory needs `WRITE_DAC`, which an AppContainer lacks
([[BPR-0833]]). Because every row runs uncontained, the three courses that do
not narrow their parent today can start doing so without meeting a container.
A row that is contained by the time this is taken has to be measured before it
is pinned.

## Scope

- Replace each of the eight copies with `e2e::private_data_root`. Keep each
  course's existing failure message and the step where a refusal stops it.
- Remove includes that the courses no longer use. Do not change the helper's
  contract, unless one of the eight needs something it lacks. If one does,
  record what it needs and why.
- Add a gate that fails when a course roots application data directly in a
  bare temporary directory, and names the course. Choose its form with
  evidence and record the reason on this card. Two candidate forms:
  1. **Fail on the symptom.** Give the application rows a
     `FAIL_REGULAR_EXPRESSION` (or an equivalent check) for
     `AGENT_WORKSPACE_RECOVERY_REFUSED`. The one deliberate emitter,
     `application_profile_management_public_path`'s `shared-parent` section,
     must stay green, either by exempting that row by name with the reason, or
     by making the deliberate line distinguishable. This catches C++, CMake
     and Python courses alike. It cannot fire in a macOS ctest run. There,
     `scripts/run_network_isolated_test.py` points `TMPDIR` at
     `/tmp/bpr-net-*/fixture`, a mode-`0700` directory that the runner
     creates for the row and deletes when the row ends. A bare
     `QTemporaryDir` made there has a private parent, which recovery
     accepts. It fires only when the course reaches application startup.
  2. **Fail on the source.** Scan `tests/end_to_end` for an
     `ApplicationDataRoot` that is taken from a `QTemporaryDir`'s path without
     the helper. This fires on every host, before anything runs. It is a text
     scanner's precision, and it cannot see Python or CMake drivers.
- Say which courses the chosen gate covers and which it cannot. [[BPR-1044]]'s
  Windows note records the same line from the Python-driven
  `application_profile_window_review_restoration_public_path`: each of its
  data roots is a child of the course's own temporary directory, which
  `os.chmod(root, 0o700)` does not narrow on Windows. It was seen only in
  runs that failed for another reason. Measure whether passing runs print it.
  If they do, either fix it here or card it separately, and state which
  choice was made.

## Acceptance

- AC1 None of the eight hand-written copies remains. Each course takes its
  temporary data root from `e2e::private_data_root`. A `grep` for
  `make_storage_root_owner_private` and `create_storage_root_owner_private`
  in `tests/end_to_end` finds no `<parent>/data` arrangement outside the
  helper. At `d4f9d07d` its other hits serve other purposes, and they stay:
  roots a driver passes in through `argv` (`device_binding_relaunch_tests.cpp:84`,
  `queue_composition_public_path.cpp:163`), store and driver roots with no
  application (`release_update_recovery_tests.cpp:33`,
  `release_update_public_path_tests.cpp:397`,
  `windows_profile_storage_startup_contract_tests.cpp:53`), the diagnostic
  sink probes (`profile_management_public_path_tests.cpp:235`,
  `profile_multi_window_public_path_tests.cpp:776`) and the `shared-parent`
  section.
- AC2 The gate fails on a planted bare root and passes on the tree:
  - A negative control reverts one course to a bare `QTemporaryDir` root. The
    gate fails and names that course.
  - The same control applied to one of BPR-1044's twelve courses also fails.
  - Neither plant is committed.
- AC3 Linux: serial and offscreen, with fresh `HOME` and XDG roots, every row
  in the table passes, apart from the known red `dif_context` row, which
  fails only in its known way. The gate passes. BPR-1044's thirteen rows
  print `AGENT_WORKSPACE_RECOVERY_REFUSED` only from the `shared-parent`
  section.
- AC4 Windows: every row in the table passes as it is registered, apart from
  known red rows, which fail only in their known ways. The gate passes, and a
  new pin or re-registration records its measured cause.
- AC5 macOS: every row in the table that is registered on macOS passes, and
  the gate passes, or the card records that the gate's form cannot run there.

## Evidence

- BPR-1044's survey and change, with its Linux and Windows runs:
  [BPR-1044](../60-done/BPR-1044-courses-rooted-directly-in-the-system-temporary-directory-trip-the-workspace-recovery-refusal.md),
  sections "Survey" and "One helper gives the twelve courses their owned
  level". The eight copies are listed there as "No C++ course that already
  keeps its root one owned level down printed it".
- The eight sites and their rows were re-read at `d4f9d07d` on
  bpr-main-linux, using `ctest --test-dir build/dev -N -V`. The Windows
  registrations were read from `scripts/verify_test_network_isolation.py`
  at the same revision.

## The eight courses take the helper, and a refusal fails its row (bpr-main-linux, 2026-09-24)

Station `bpr-main-linux` (WSL2 Ubuntu, GCC 15.2, Qt 6.11.2, `build/dev`
Debug from the `dev` preset, `-Werror`), claimed at `47dc437d`. The change
landed as **`ff9bc9e2`** on top of `3db65162`, which touches only another
card. Every run below was serial and offscreen: `ctest -j1 -V`,
`QT_QPA_PLATFORM=offscreen`, `DISPLAY` and `WAYLAND_DISPLAY` unset, a fresh
`HOME` and fresh XDG data, config, cache, state and runtime directories that
were removed afterwards, and `TMPDIR` unset, so every `QTemporaryDir` was made
in `/tmp`. The logs are station records under
`build/logs/bpr-main-linux-20260924/bpr-1050/` and are not committed.

### The courses

Each copy is now one call to `e2e::private_data_root`. Each course keeps its
failure message, and a refusal still stops it at the same step.

| File | What changed |
| --- | --- |
| `agent_chat_composition_public_path.cpp` | The helper replaces the copy. `agent-chat-composition-failure:private-test-root` is kept. The root is now canonical and native |
| `agent_finding_composition_public_path.cpp` | The same, keeping `report_failure("private-test-root")` |
| `agent_invocation_composition_public_path.cpp` | Only the `QTemporaryDir` branch takes the helper, which now also narrows the parent. The two `argv` branches still create the root and narrow it, as before. `agent-invocation-composition-failure:private-test-root` is kept |
| `github_application_liveness_tests.cpp` | The helper replaces the copy. The course had two messages, one for the canonical form and one for privacy. The helper settles both, so one message is left: `the data root is owner-private` |
| `large_review_benchmark.cpp` | The helper replaces the copy. The root is now canonical as well as native. `large-review-benchmark-failed: private data root parent` is kept |
| `profile_management_public_path_tests.cpp`, main root | The helper replaces the copy, so the parent is now narrowed too. The `shared-parent` section is unchanged, and it still builds its `0775` parent inside the temporary directory |
| `profile_second_window_close_public_path_tests.cpp` | The helper replaces the copy. The two messages are now one |
| `release_maintenance_public_path_tests.cpp` | The helper replaces the copy, so the parent is now narrowed. Under `QUALIFICATION_TARGET_ROOT` the temporary directory is still made there |

Includes that the courses no longer use were removed. The helper's contract
is unchanged, because none of the eight needed anything it lacks.

### The gate: the application's own refusal line fails the row

**Chosen form: fail on the symptom, in the registration seam.**
`bpr_add_network_isolated_test` (`cmake/BprTestIsolation.cmake`) now adds
`AGENT_WORKSPACE_RECOVERY_REFUSED` to the error expressions of every row it
registers, beside `PUBLIC_PATH_FAILED` and `TEST_RUNNER_CHILD_EXIT_FAILED`.
The failing row is the course, and the line names the root it refused. A row
whose subject is the refusal registers with the new keyword
`EXPECTS_WORKSPACE_RECOVERY_REFUSAL`. That keyword labels the row
`bpr_expects_workspace_recovery_refusal` and drops only this entry.
`application_profile_management_public_path` takes it where its
`shared-parent` section is compiled, which is everywhere except Windows. On
Windows the row keeps the entry for its main root.
`scripts/verify_test_network_isolation.py` pins the entry on every row in
CTest's own manifest, and it pins the one row allowed the exemption. The
exemption check is exact in one direction only, so a recorded row that is
configured without the label is simply held to the stricter rule.
`application_automated_run_budget`, whose subject is a failed run, now carries
this one entry and still carries neither verdict entry.

**Why this form, and not the source scan:**

- **It is exact.** The application prints the line only when recovery refuses.
  In the 164-row sweep below, the line appeared once, from the deliberate
  section. No row had to be exempted for noise.
- **Nothing has to remember it.** Every row goes through this one seam, so a
  new course inherits the check. The manifest gate fails if a row-level
  `FAIL_REGULAR_EXPRESSION` replaces it.
- **It does not depend on the language.** A C++, CMake or Python course fails
  the same way, provided the line reaches CTest.
- **A source scan would have had to follow each course's own spelling.** The
  eight copies spelled the same arrangement in five ways, and the hazard is a
  data flow from `QTemporaryDir` to `ApplicationDataRoot`, not a token. A scan
  would see only C++, so it could not see the Python course below, which
  Windows refused.
- **What the scan would add, a macOS check, guards something macOS accepts.**
  Under ctest the macOS runner hands each row a private `TMPDIR`, and outside
  ctest macOS's per-user `TMPDIR` is private too ([[BPR-0856]]'s comment in the
  large-review benchmark). So a bare root is not refused there, and there is
  nothing to fail on.

**What it covers.** It covers every row on Linux and Windows whose output
includes the application's standard error. That is every C++ course that ctest
runs directly, which includes the eight courses above and [[BPR-1044]]'s
twelve. It also covers every CMake or Python driver that relays its child's
standard error.

**What it cannot cover.**

- **A driver that captures its child's standard error and does not relay it.**
  Most `cmake -P` contracts capture `ERROR_VARIABLE`, and many Python drivers
  use `capture_output`. Those courses take their roots from the shared helpers
  (`bpr_private_data_root`, `contained_temporary_directory`).
- **Any macOS run under ctest.** This limit is also documented beside the seam
  and in `docs/development/build-and-test.md` ("A course's data root sits one
  owned level down").

### The Python-driven restoration course

`application_profile_window_review_restoration_public_path`'s driver
(`profile_window_review_restoration_contract.py`) printed a phase's standard
error only when the phase failed. So the gate could not see this course, and
neither could anyone reading a passing run. It now relays each passing phase's
standard error too. Standard error is the phase's diagnostic, and it was
already printed on failure. The vault material travels on standard output,
which is still never printed. The driver also narrows its temporary directory
with `make_root_private`, in addition to `os.chmod(root, 0o700)`. On Linux the
added call has nothing to change. On Windows it writes the descriptor that the
stores define, which `os.chmod` did not do.

- **Linux, passing run at the candidate:** passed in 99.27 s. All 34 phase
  lines were relayed, with **no** refusal line.
- **Negative control 1**, the root's parent made `0777` in place of the new
  call: the row **failed** on
  `Regex=[AGENT_WORKSPACE_RECOVERY_REFUSED]`, with 34 lines such as
  `AGENT_WORKSPACE_RECOVERY_REFUSED:/tmp/bpr-0709-restart-2oxrl84b/auth/agent-workspace-ownership-v1:repair-required`.
- **Negative control 2**, the same parent with the relay also taken out: the
  row **passed**, and 0 lines were visible. So before this change, the course
  could hide a refusal from every phase.
- Both edits were then undone. The file's SHA-256 matches the candidate again,
  and `grep -rc BPR1050_NEGCTL src tests` finds nothing.

Whether passing Windows runs printed the line was **not measured here**. It
needs the Windows station (below). The fix is in this card because the cause
is the same one this card fixes, and the helper already existed. The Windows
run decides whether it was needed. If the negative control there does not
bring the line back, record it, and the narrowing stays as a harmless
duplicate of the chmod.

### Verification (Linux)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev`, after reconfiguring for the CMake change | Exit 0, no `warning:` line |
| Manifest: `FAIL_REGULAR_EXPRESSION` on all 423 rows | 420 rows carry the three entries. `rfc0017_release_absence_negative_control` carries `PUBLIC_PATH_FAILED` and the refusal. `application_automated_run_budget` carries the refusal only. `application_profile_management_public_path` carries the two verdict entries and the label |
| AC2, one of the eight: `github_application_liveness_tests.cpp` planted with the bare `QTemporaryDir` path as its root | `application_poller_liveness` **failed**, `Error regular expression found in output. Regex=[AGENT_WORKSPACE_RECOVERY_REFUSED]`, in 0.21 s, after printing both of its pass markers. The line names `/tmp/better_pull_review_github_application_liveness_tests-lXAqiD/agent-workspace-ownership-v1` |
| AC2, one of [[BPR-1044]]'s twelve: `application_logging_tests.cpp` planted the same way | `application_logging` **failed** on the same expression in 5.53 s, after `application-logging-tests:pass`. The line names `/tmp/application_logging-jakKuV/agent-workspace-ownership-v1` |
| Plants removed | Both files' SHA-256 match the candidate, both targets were rebuilt, and nothing was committed |
| AC3: the 164 rows of [[BPR-1044]]'s survey, which include every row in the table and the thirteen (`sweep-candidate.log`) | 162 of 164 passed, in 1,173.66 s. The two failures are the known ones. `application_agent_finding_dif_context_contract` ([[BPR-0966]]) failed on `TEST_RUNNER_CHILD_EXIT_FAILED` with `dif-context-evidence failed=dif-comment-ready-after-conversion dif-acknowledge-after-acceptance`. `application_uiforetw_selected_diff_contract` ([[BPR-1036]]) failed with `UIFORETW_CAPTURE_LAYOUT_FAILED:987:842 width_floor=750 height_floor=850`. **One** refusal line, from the deliberate section: `/tmp/profile_management_public_path-bEWvdy/shared-parent/BetterPullReview/agent-workspace-ownership-v1:repair-required`. The row passed on its exemption |
| The table's rows in that sweep | 31 of the 32 rows passed, in 0.20 to 99.14 s. The 32 are the 29 rows in the table, the two crash-recovery rows that use the invocation binary, and the restoration course. The one failure is `dif_context`, which failed only in its known way |
| The thirteen in that sweep | 13 of 13 passed, in 1.76 to 28.94 s |
| The restoration course at the final file (the sweep ran the earlier driver) | Passed, as above |
| `core_boundary_contract`, `architecture_boundaries`, `architecture_boundary_contract`, `conditional_branch_identifiers`, `conditional_branch_identifier_contract` and the eight `test_network_isolation_*` rows | 13 of 13 passed |
| `documentation_structure`, `documentation_issue_graph_contract`, `skip_report_contract`, `coding_format_check` (796 files, `/usr/bin/clang-format` 22.1.8), `test_network_isolation_structure_contract`, `test_network_isolation_manifest` | 6 of 6 passed |
| `tests/scripts/verify_test_network_isolation_tests.py` | 65 tests passed, and 1 was skipped because it is macOS-only. That includes the new `test_workspace_refusal_rows_drop_only_the_refusal_entry` |
| `python3 scripts/verify_documentation.py --root .`, `tests/scripts/verify_documentation_tests.py`, `git diff --check` | `DOCUMENTATION_OK`, 64 tests passed, clean |
| `_coding-standard/scripts/tidy.py` on the eight translation units (clang-tidy 21.1.8) | No finding on a changed line. Its findings are older than this card, for example the naming findings in `large_review_benchmark.cpp` |

The sweep left 10 entries in `/tmp`: eight `QuillBackendLock<pid>` files,
one `bpr-mcp-canary-*` and one `bpr-public-path-*`. That is [[BPR-1051]]'s
defect. All ten were created inside the sweep's window, and they are recorded
under that card's station logs. They were removed by exact name.

### Acceptance state

- **AC1: pass.** None of the eight copies remains. In `tests/end_to_end`,
  `make_storage_root_owner_private` and `create_storage_root_owner_private`
  are now found only in the helper, in the invocation course's `argv` branch,
  and in the hits the criterion lists as serving other purposes. Those are
  `device_binding_relaunch_tests.cpp:84`, `queue_composition_public_path.cpp:163`,
  `release_update_recovery_tests.cpp:33`, `release_update_public_path_tests.cpp:397`,
  `windows_profile_storage_startup_contract_tests.cpp:53`, the two sink probes
  and the `shared-parent` section. The other hit is a comment in
  `bpr_narrow_owner_private.py`.
- **AC2: pass.** Each plant failed its row and named the course, and neither
  plant was committed.
- **AC3: pass on Linux.** Every row in the table passed except `dif_context`,
  which failed only in its known way. The gate passed. Across the thirteen,
  the line came only from the `shared-parent` section.
- **AC4: owed by bpr-main-win.**
- **AC5: owed by bpr-main-mac.** The gate's form **cannot fire in a macOS
  ctest run**, for the reasons above, so on macOS the criterion rests on the
  rows passing and on the manifest gate.

### Still owed: the Windows run (bpr-main-win)

On a head that contains `ff9bc9e2`, reconfigured, because the seam changed,
and rebuilt:

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev -j1 -V -R "^(test_network_isolation_manifest|test_network_isolation_structure_contract)$"
ctest --preset windows-dev -j1 -V -R "^application_(agent_(chat|finding|invocation)_.*_contract|agent_crash_recovery|agent_materialization_crash_recovery|poller_liveness|large_review_benchmark|unavailable_diff_public_path|approximate_diff_public_path|profile_management_public_path|profile_second_window_close_public_path|release_maintenance_public_path|profile_window_review_restoration_public_path)$"
ctest --preset windows-dev -j1 -V -R "^application_(device_binding_launch_renewal|device_binding_restart|setup_destination_public_path|review_submission_composition_public_path|attended_recent_replay|profile_startup_public_path|selected_file_safety_public_path|logging|personal_discovery_public_path|profile_multi_window_public_path|profile_two_account_isolation_public_path|release_update_public_path|profile_management_public_path)$"
ctest --preset windows-dev -j1 -R "^application_"
```

Expect:

- **Manifest gate.** It passes. On Windows,
  `application_profile_management_public_path` carries no
  `bpr_expects_workspace_recovery_refusal` label, and the verifier accepts
  that.
- **The table's 32 rows and the thirteen.** They pass as registered, apart from
  known red rows, which should fail only in their known ways. All of them are
  `WINDOWS_UNCONTAINED`, so narrowing the parent meets no container. If a row
  fails on `AGENT_WORKSPACE_RECOVERY_REFUSED`, record the line and its path.
- **The restoration course.** Its phases' standard error now reaches the log.
  Count its refusal lines, which should be 0. Then run a negative control:
  locally, and without committing, replace `make_root_private(root)` in
  `profile_window_review_restoration_contract.py` with `pass` and run the row
  again. If the row now fails on the refusal, then passing Windows runs did
  print the line, and this card's narrowing is what removes it. Record either
  outcome, then restore the file.
- **The whole `application_` sweep.** It shows whether any other Windows row
  prints the line. Every such row now fails. Record each one with its path, and
  report its title to the coordinator for a follow-up card: it would be the
  same cause, in a course this card did not reach.
- **A new pin or re-registration** records the cause that was measured.

### Still owed: the macOS run (bpr-main-mac)

On a head that contains `ff9bc9e2`, reconfigured and rebuilt:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev -j1 -V -R '^(test_network_isolation_manifest|test_network_isolation_structure_contract)$'
ctest --preset dev -j1 -V -R '^application_(agent_(chat|finding|invocation)_.*_contract|agent_crash_recovery|agent_materialization_crash_recovery|poller_liveness|large_review_benchmark|unavailable_diff_public_path|approximate_diff_public_path|profile_management_public_path|profile_second_window_close_public_path|release_maintenance_public_path|profile_window_review_restoration_public_path)$'
```

Expect:

- The manifest gate passes.
- Every row that is registered on macOS passes, apart from known red rows.
- `application_profile_management_public_path` prints the refusal once, from
  its `shared-parent` section, and passes on its label. No other row prints
  it. On macOS that does not show that the gate works, because the gate cannot
  fire there. It only confirms that no macOS row hit the refusal.

## The whole suite under the gate (bpr-main-linux, 2026-09-24)

This is the integration run for this card and [[BPR-1051]]. `09f3d48f`
contains `ff9bc9e2`. All 423 configured rows ran serially and offscreen, with
a fresh `HOME` and fresh XDG roots, and with `TMPDIR` unset. So every
temporary directory was made in `/tmp`, the case the gate is for.

- **418 of 423 rows passed**, in 2,134.42 s.
- **The five failures are the known red rows.** They are
  `qt_anonymous_public_preview_scale_100`, `_125` and `_200` ([[BPR-1023]]),
  `application_agent_finding_dif_context_contract` ([[BPR-0966]]) and
  `application_uiforetw_selected_diff_contract` ([[BPR-1036]]).
- **No row failed on `AGENT_WORKSPACE_RECOVERY_REFUSED`.** The log holds one
  such line, from profile management's `shared-parent` section:
  `/tmp/profile_management_public_path-ZrGkMn/shared-parent/BetterPullReview/agent-workspace-ownership-v1:repair-required`.

So on Linux no other row prints the refusal, and adding the entry to every row
turned none of them red. The log is
`build/logs/bpr-main-linux-20260924/bpr-1051/full-sweep-09f3d48f.log`, a
station record that is not committed.
