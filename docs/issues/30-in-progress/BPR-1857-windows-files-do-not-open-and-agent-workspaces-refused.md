---
schema: "implement-rfc.card.v1"
id: "BPR-1857"
title: "On Windows no file opens, and agent workspaces are refused, without saying why"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["windows", "linux", "macos"]
metadata: {"reservation": "docs/issues/_reservations/BPR-1857--BPR-1857--58190ccb501f.json", "kind": "defect", "found_by": "owner photos, 2026-11-04 (corporate Windows machine, release 0.4.x)", "related": ["BPR-1791", "BPR-1790", "BPR-1789"], "related_rfcs": ["RFC-BPR-0015", "RFC-BPR-0033"]}
acceptance: [{"criterion": "AC1", "outcome": "pending", "revision": "784f97eeaef843eb3a76e93bb51295ea5f159e86", "evidence": {"kind": "command", "detail": "macOS arm64 dev, 784f97eea: core_epoch_policy, application_epoch_host, qt_epoch_comparison_scale_100, qt_workbench_window_scale_100, gh_cli_transport, gh_cli_git_smart_transport, agent_workspace_ownership, application_agent_crash_recovery, application_agent_materialization_crash_recovery, application_review_round_trip_composition_contract, application_setup_destination_public_path, application_profile_management_public_path, application_home_open_feedback_public_path, application_terminal_diagnostics pass. Windows build and the same selection on bpr-main-win pending."}}, {"criterion": "AC2", "outcome": "pending", "revision": "784f97eeaef843eb3a76e93bb51295ea5f159e86", "evidence": {"kind": "command", "detail": "Pending: a GitHub CLI session on bpr-main-win opens a file of a BPR-Test pull request."}}, {"criterion": "AC3", "outcome": "pending", "revision": "784f97eeaef843eb3a76e93bb51295ea5f159e86", "evidence": {"kind": "command", "detail": "Pending: the owner's machine shows the named refusal (check, folder, account) or recovers."}}]
---

# BPR-1857: Windows files do not open; agent workspaces refused without a reason

## Observed

The owner, 2026-11-04, on a Windows work machine running a 0.4.x release
against an organization's `game-engine` repository (photos in the session):

- Review shows **No file open**. The status bar says "No exact comparison is
  ready for this file. Check Review history, then retry."
  (`workbench_host_failure::selected_file_comparison_unavailable`).
- The Review history selector's tooltip says "File comparisons unavailable.
  Review history could not be updated. The exact comparison is unavailable.
  Refresh to try again. (no pair yet)". Refresh changes nothing. Comments,
  description and the file list all load, so GitHub API reads work.
- Home says "Disposable agent workspaces are unavailable: their folder needs
  repair or has reached its limit." BPR-1789/BPR-1790 saw the same refusal,
  `repair-required`, on the owner's other Windows machine on 2026-10-27.
- Git 2.55.0.windows.3 and gh 2.97.0 are installed. Both are above the minimums
  (Git 2.49, gh 2.96), and the version parsers accept a `.windows.3` suffix.

## Root cause 1: GitHub CLI cannot fetch Git objects on Windows (found in code)

"The exact comparison is unavailable" is `epoch_reason_code::unavailable`. The
host produces it from `core::epoch_failure::unavailable`, and
`smart_transport_failure` mapped every Git-object transport failure that was
not a version or capability probe to it. On Windows a GitHub CLI session
always fails there, before it sends any request:

- `gh_cli_git_smart_transport.cpp` made every clone root as
  `QTemporaryDir("/private/tmp/bpr-cli-git-XXXXXX")`. On Windows that names
  `\private\tmp` on the current drive, which does not exist. So the directory
  is invalid and the step `temporary_store` returns `failure::unavailable` on
  every attempt.
- Behind it, `compile_gh_cli_git_clone_request`,
  `compile_gh_cli_exact_blob_request` and `compile_gh_cli_exact_blobs_request`
  returned `std::nullopt` on anything but macOS (`#if !defined(__APPLE__)`), and
  the clone root had to sit directly in `/private/tmp`.

BPR-1791 enabled **Use GitHub CLI** on Windows and Linux, and its own
"Disclosed differences" says Git-object acquisition stayed macOS-only. The read
warning's text says so, but the comparison failure did not. Everything that
matches the report follows: it is deterministic, Refresh cannot help, API reads
work, and every file of every pull request fails. A corporate organization with
single sign-on is the natural place for a GitHub CLI session. Whether the
owner's session is GitHub CLI or browser sign-in is not yet confirmed (see
Pending).

Every condition that yields the reported text on Windows (the epoch preparation
stages in `epoch_application_host.cpp`, all collapsing into "The exact
comparison is unavailable" before this card):

| Stage | Failure that read "unavailable" |
|---|---|
| `catalog_open`/`catalog_prepare` | `epoch_catalog_failure::invalid_path`, `invalid_permissions`, `unavailable`, `platform_unsupported`, `cancelled` |
| `cache_open`/`revision_prepare` | `git_cache_failure::invalid_path`, `lock_unavailable`, `spool_failure`, `cancelled` |
| `smart_acquire`/`protocol_acquire` | any `git_smart_transport_failure` at a step other than the four tool probes: authentication, permission, network, certificate, proxy, redirect, timeout, request/response limit, malformed response, consumer rejected, collision, and `unavailable` itself, which covers the Windows `temporary_store` failure above, a clone that exits non-zero, and a process runner that cannot start |
| any | an exception thrown inside an effect |

The workbench's own sentence comes from
`workbench_application_host.cpp::submit_selected_diff`: when
`SelectedDiffs->context(authority)` has no accepted comparison, it publishes
`selected_file_comparison_unavailable`. So the file failure is downstream of the
review-history failure and has the same cause.

## Root cause 2: agent-workspace refusal (diagnosed in code; the owner's check is not yet known)

`recover_installed_agent_workspaces` refuses at startup for any of these
(`installed_git_process.cpp`):

| Check (now named) | Condition |
|---|---|
| `invalid-path` | registry or neighbourhood not absolute, normal, bounded, or one inside the other |
| `neighbourhood-parent-shared` | the folder holding `<data root>-agent-workspaces-v1` (by default `%LOCALAPPDATA%\BetterPullReview`) is not canonical, is a reparse point, is not owned by the user, or has an allow entry granting write to anyone but the user, the package, SYSTEM, Administrators or OWNER RIGHTS, or a non-plain allow entry |
| `neighbourhood-not-private` | the neighbourhood exists without the exact owner-private descriptor |
| `neighbourhood-not-created` | it could not be created |
| `data-folder-not-private` | the data root lacks the exact owner-private descriptor |
| `registry-not-private` | `agent-workspace-ownership-v1` exists without it, or could not be made |
| `registry-unexpected-entry` | the registry holds anything that is not `<32 hex>.jsonl` |
| `unrecorded-leftover` | the neighbourhood holds `betterpullreview-agent-*` or `.bpr-cleanup-*` that no record names |
| `record-not-private` | a record is not an exact private single-link file, or cannot be opened |
| `record-unreadable` | a record cannot be parsed, or its workspace fails exact identity cleanup |
| `limit-reached` | more than 64 records, or 4096 neighbourhood entries |
| `lock-unavailable` | the registry's process lock, or a record's lock, cannot be taken |

The data root is made owner-private by the SQLite adapter, but the folder above
it is created by `create_directories` and inherits its ACL from
`%LOCALAPPDATA%`. Corporate images commonly add entries there. Two of those
cases are now decided:

- **Inherit-only entries are a false refusal; fixed.** An entry flagged
  `INHERIT_ONLY_ACE` (CREATOR OWNER, or a group granted only on what is created
  inside) grants nothing on the folder itself. The neighbourhood is created
  with a protected descriptor that inherits nothing, so such an entry cannot
  reach it. The walk now skips them.
- **A real write grant to another account stays refused (deliberate).** That
  account could rename the neighbourhood and substitute its own, which is what
  the owner-private rule exists to stop. We cannot tell an IT administrators'
  group from "Authenticated Users" by its SID, so a corporate grant is not
  admitted. The refusal now names the account and the folder. The user can
  remove that grant from BetterPullReview's own folder, or ask IT. The same
  holds for a folder owned by `BUILTIN\Administrators`, which happens when the
  app first ran elevated. The message says to take ownership.

Which check the owner's machine hit is unknown until the new build runs there
or the `icacls` output below is supplied.

## Built

- **GitHub CLI Git objects on Windows and Linux.** `gh_cli_git_temporary_parent()`
  (`gh_cli_transport.{hpp,cpp}`) is `/private/tmp` on macOS, as before, and the
  canonical `std::filesystem::temp_directory_path()` elsewhere. The three
  clone roots are made there (`make_clone_root`, `clone_root_path`), and the
  clone request admits a root only directly inside it. The three
  `#if !defined(__APPLE__)` refusals are gone. Git paths in arguments and the
  alternates file are UTF-8 with forward slashes (`git_path_text`), never a
  narrow `string()`. The read warning's non-macOS text says full diffs run.
- **The fetch failure says what failed.** `core::epoch_failure` and
  `epoch_reason_code` gain `fetch_sign_in_refused`, `fetch_network`,
  `fetch_limit`, `fetch_failed` and `fetch_storage`, appended so earlier
  ordinals are unchanged. `smart_transport_failure` maps by failure kind, then
  by step (`temporary_store`, `process_runner` and `local_store` are the
  folder; `clone` is the clone), and leaves `unavailable` for what is left. Each
  has its own sentence and remedy in `epoch_comparison_control.cpp`, and the
  diagnostic log names them.
- **The file's sentence says the reason.** `epoch_comparison_control::problem_text()`
  returns the comparison's, history's or last diagnostic's reason while review
  history is failed or unavailable. `workbench_window` appends it to "No exact
  comparison is ready for this file." on Home and in the status line.
- **The workspace refusal names its check, folder and account.**
  `workspace_recovery_refusal`/`workspace_refusal_check` and
  `workspace_refusal_check_name` (`agent_workspace_ownership.hpp`), filled by
  every refusal point. On Windows the account is resolved by
  `LookupAccountSidW`, falling back to the SID string. The terminal line becomes
  `AGENT_WORKSPACE_RECOVERY_REFUSED:path=omitted:<failure>:check=<name>`; the
  path stays off the terminal. Home, the status line and the agent panel say
  the check's own sentence with the folder and what to do
  (`agent_workspace_refusal_text`, `set_agent_workspaces_unavailable(bool, QString)`).
  The sentences never move, change or delete anything.

### A guarded Repair: considered, deferred

A Repair that moves unproven leftovers aside after confirmation would help
only `unrecorded-leftover`, `registry-unexpected-entry`, `record-*` and
`neighbourhood-not-private`. The likely corporate cases are ACL and ownership
findings on a folder above the data root, where moving leftovers does nothing,
and changing a folder's ACL for the user is a larger trust decision. The named
sentence already says which file or folder to move, and nothing is deleted. A
Repair card should follow once the owner's actual check is known.

## Acceptance

- AC1. Focused tests pass: the new fetch reasons (core, application, Qt), the
  file sentence naming the reason, the clone root parent on every platform, and
  the named refusal checks (POSIX leftover and shared parent; Windows
  inherit-only admitted, a write grant refused and named). Windows: the same
  suite on bpr-main-win.
- AC2. On bpr-main-win, a GitHub CLI session opens a file of a BPR-Test pull
  request, and its Review history reaches "pair ready".
- AC3. On the owner's work machine the new build either opens files and
  recovers workspaces, or says which check refused, naming the folder and the
  account.

## Verification

macOS arm64, dev preset, 784f97eea:

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R '^(core_epoch_policy|application_epoch_host|qt_epoch_comparison_scale_100|qt_workbench_window_scale_100|gh_cli_transport|gh_cli_git_smart_transport|agent_workspace_ownership|application_agent_crash_recovery|application_agent_materialization_crash_recovery|application_review_round_trip_composition_contract|application_setup_destination_public_path|application_profile_management_public_path|application_home_open_feedback_public_path|application_terminal_diagnostics)$'
```

All 14 pass. `architecture_boundaries` fails on a `QToolTip::showText` in
`workbench_window.cpp:5028`, which predates this card (8d448697b, BPR-1850)
and is unchanged by it. `coding-tidy` was not run. No live GitHub call was
made.

New evidence in that selection: each fetch failure keeps its reason (core,
application host, Qt sentence); the file's status names review history's
failure reason; the clone root parent exists and is canonical on the running
platform; the ownership harness's `recover-check` names
`neighbourhood-parent-shared` for a group-writable parent and
`unrecorded-leftover` for an unrecorded workspace, which is left intact; the
startup-repair course sees the named `record-unreadable` sentence. The Windows
ownership cases (an inherit-only Everyone entry admitted, an applying one
refused and named) run only on Windows.

### bpr-main-win, bbaeba9ac (Debug, MSVC 14.51)

The whole tree compiles, including every Windows-only line of this card.
`core_epoch_policy`, `application_epoch_host`, `qt_epoch_comparison_scale_100`,
`qt_workbench_window_scale_100`, `gh_cli_transport` and
`application_review_round_trip_composition_contract` pass.
`gh_cli_git_smart_transport` and `application_terminal_diagnostics` are not
registered on Windows.

- `agent_workspace_ownership` failed on the card's own inherit-only course.
  The production check was right. The course's error was its layout: in the
  driver's plain layout the folder holding the neighbourhood is also the
  registry's data folder. The inherit-only entry was correctly skipped as the
  neighbourhood's parent, but it broke the data folder's exact descriptor, so
  `data-folder-not-private` refused. In the product the data folder is a
  private child of its own. The course now asserts that named refusal, then
  the product's layout (`recover-split`, registry in a private `data` child),
  which recovers.
- `application_agent_crash_recovery` timed out at `crash-running` after 60 s.
  This card changes nothing on that path: no refusal is stricter, and the
  account lookup runs only on a refusal. Whether main also fails there is
  pending. `process_kill_support.py` now prints the fixture's output when a
  checkpoint is missed.

## Pending

- The owner: is the work machine's account **GitHub CLI** (Accounts shows the
  transport)? Is `%LOCALAPPDATA%`/`%APPDATA%` redirected (OneDrive, a network
  share, folder redirection)? And `icacls` of the folders below, which only
  their machine can give.
- bpr-main-win: the build and courses below, then AC2 live on BPR-Test.
- Not compiled here: every `_WIN32`/`Q_OS_WIN` line of this change
  (`windows_account_text`, the inherit-only skip, the Windows clone root).

## 0.4.2 release integration

The owner selected inclusion of BPR-1857 in the release conversation on
2026-11-04. Only merge 49da209d3 relative to its first parent is carried onto
frozen release source 24a94242a; later main features are excluded. Scoped
engineering self-review covers the complete incoming app-manifest delta:
`github_application.cpp` passes the workspace refusal record through the
existing decision lane, retains a path-omitted terminal line with a named
check, and projects the check, folder and account into the existing window and
agent error surfaces. It adds no provider, credential, grant, publication or
source-inventory path. The production mutation branch remains byte-identical
(`70fe8a7e`). The full application and production manifest pins are resealed
only for these reviewed combined bytes; the original drift result is retained
in the external release run. Native affected-path checks, live CLI comparison,
all-platform compilation and full qualification remain pending for the new
release candidate. This integration does not close AC2 or AC3.
