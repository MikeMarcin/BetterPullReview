---
schema: "implement-rfc.card.v1"
id: "BPR-1050"
title: "Move the eight courses that hand-write the owner-private data-root arrangement onto bpr::e2e::private_data_root, and fail a gate when a course roots data in a bare temporary directory"
status: "Ready"
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
