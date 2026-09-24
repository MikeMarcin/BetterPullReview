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
