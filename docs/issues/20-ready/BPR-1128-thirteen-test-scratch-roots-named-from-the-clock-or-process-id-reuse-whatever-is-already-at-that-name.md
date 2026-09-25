---
schema: "implement-rfc.card.v1"
id: "BPR-1128"
title: "Thirteen test scratch roots named from the clock or process id reuse whatever is already at that name, and an agent-canary marker is removed before it is made"
status: "Ready"
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

- Every root in the table is made by one shared helper that any test can use:
  under a random name, created only if nothing is at that name,
  owner-private before anything is written in it (mode `0700` on POSIX),
  owned from creation by [[BPR-1121]]'s `scratch_guard`, and removed only by
  the test that made it. [[BPR-1126]]'s `tests/sqlite_contract/scratch_root.hpp`
  is that helper today, but it narrows with the SQLite adapter's rule, which
  the Git contracts do not link. The shared part moves to `tests/support/`,
  and the SQLite contracts keep their rule on top of it.
- The canary's descendant writes its marker into a directory the test made
  that way, instead of at a name the test first removed.
- Sources this station cannot build are not edited blind. Row 13 and the
  Windows-only marker are built only on Windows. The card records the exact
  change for that station.
- `tests/AGENTS.md` names the shared helper.

## Acceptance

- AC1: Each root in rows 1 to 12, and the directory holding the canary's
  marker, is made by the shared helper: under a random name, created only if
  nothing is at the name, owner-private before anything is written in it,
  and owned from creation. None of these sources reuses what is at a name,
  or removes anything before it makes a root.
- AC2: Negative control. At `HEAD`, a trial hook pins each source's name to a
  value the trial knows. The hook is never committed. A sentinel directory
  planted at the first pinned name is reused by the row, and removed at its
  end. The card records what `HEAD` does with a link planted there that
  points to a sentinel directory the trial owns. For the marker, a planted
  sentinel file is removed. A link put back at the name while the course runs
  is written through by the descendant. With the candidate, every plant, and
  the sentinel directory's contents, are left exactly as they were.
- AC3: The Linux rows pass with the candidate and print the same lines as at
  `HEAD`, apart from random names and timings. A passing run leaves no
  root. Forced failures, made with [[BPR-1121]]'s trial mechanism, leave none
  either. With unwinding turned off, the same failures do leave roots: the
  negative control that shows the trial can see a leak.
- AC4: The change is portable. The sources build, and their rows pass, on
  macOS and Windows. The helper's Windows branch is compiled first on
  Windows. On the Windows station, row 13 and the Windows-only marker are
  changed as this card records, and `qt_agent_launch_canary` passes.
- AC5: Gates: `/usr/bin/clang-format` 22, `git diff --check`,
  `architecture_boundaries`, `msvc_secure_crt_contract`, `DOCUMENTATION_OK`,
  and no scoped `tidy.py` finding on a changed line.

## Boundaries

- Test sources and test support only. No product source, no row
  registration and no composition pin.
- The two macOS-only roots are BPR-1127's, the Mac integrator's card:
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
