---
schema: "implement-rfc.card.v1"
id: "BPR-2293"
title: "Retained agent workspaces expire after their session ends, after N days unused and under a total-size cap, oldest first, and Settings shows their total size with Remove"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2293--BPR-2294--e7efc87e257e.json","parent":"BPR-2288","origin":"split from BPR-2291 (2026-11-19): BPR-2288 Change 5's expiry and AC6","related_cards":["BPR-2288","BPR-2289","BPR-2291","BPR-2246","BPR-2261"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "766f94c64", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(retained_workspace_expiry|core_agent_workspace_expiry)$'", "detail": "retained_workspace_expiry, over the real SQLite catalog and installed Git with seven retained workspaces: the policy sweep (as profile start runs it, cap 1 GB) removes the one unused past seven days, the one whose conversation moved to another revision, the older of two 600 MB workspaces over the cap, and one this run made (through its live owner); keeps one a turn's lease holds although eight days old, and leaves one whose ownership marker does not verify (RETAINED_WORKSPACE_SWEEP:count=3:removed=4:in_use=1:failed=1); records and folders of the removed are gone. core_agent_workspace_expiry: oldest first, the owner's 13 x 8.7 GB leave two under 20 GB, in-use counts and stays, ties by index."}}, {"criterion": "AC2", "outcome": "pass", "revision": "766f94c64", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(qt_agent_setup_view|qt_settings_window.*|retained_workspace_expiry|sqlite_agent_setup_storage)$'", "detail": "qt_agent_setup_view: 'Retained agent workspaces: 3, 17.2 GB. 1 is in use.', Remove… asks 'Remove 2 agent workspaces?', Cancel sends nothing, confirming sends Remove, the line then says what is left with the answer beside it; the cap and days are sent. retained_workspace_expiry: Remove removes every one not in use (RETAINED_WORKSPACE_REMOVE:count=2:removed=2:in_use=1:failed=1). sqlite_agent_setup_storage: the policy starts at 7 days and 20 GB and reads back changed."}}, {"criterion": "AC3", "outcome": "pass", "revision": "766f94c64", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^retained_workspace_expiry$'", "detail": "After the sweep the removed workspace's conversation is selected again with no workspace, its pull request key leads to none, and its next turn makes and stores a fresh workspace at another path."}}, {"criterion": "AC4", "outcome": "pass", "revision": "766f94c64", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^architecture_boundaries$' && python3 scripts/verify_documentation.py --root .", "detail": "architecture_boundaries passed; DOCUMENTATION_OK. clang-format 22 and clang-tidy (warnings as errors) clean on every changed C++ line."}}]
---

# BPR-2293: Retained agent workspaces expire under a size cap, and Settings shows their size

Part of [[BPR-2288]] (Change 5's expiry, AC6). Split from [[BPR-2291]], which
landed the file view's fetching state, the progress bound and the
whole-history size rule.

## Why

A native Agent-tab workspace is retained for its conversation
(`RetainForSession`) and recorded on the native agent session in the agent
setup store (`native_agent_session::Workspace`). Nothing removes it: the
owner found 13 of them, the oldest eight days old, holding 113 GB under
`BetterPullReview-agent-workspaces-v1/`. [[BPR-2289]] stops new ones packing
all of history; the ones already on disk, and the steady growth of one per
pull request binding, remain.

## Change

1. **Inventory.** List the retained workspaces the store records, with each
   one's last use (the last turn that reattached it) and its size on disk
   (a bounded walk that never follows links), on an effect worker.
2. **Expiry policy (core).** A pure decision over `{workspace, last use, size,
   in use}` and the policy `{days unused, total cap}`: remove those whose
   conversation the reviewer ended or deleted, those unused for more than N
   days (default 7), then the oldest beyond the total cap (default 20 GB).
   A workspace a turn holds (`try_lock_workspace` fails) is never chosen.
3. **Removal.** Through the existing exact cleanup
   (`cleanup_agent_workspace` and the installed-Git process's exact removal),
   which refuses a substituted path; the session's `Workspace` is cleared in
   the same store revision, so its next turn makes a fresh one. A workspace
   whose repository's cache actor is not open is removed through the same
   exact cleanup without one, or recorded as pending until it is.
4. **When.** At profile start (after recovery), when a conversation ends or
   is deleted, and once a day while the app runs.
5. **Settings.** An Agent row: "Retained agent workspaces: 3, 17.2 GB", with
   **Remove** (all not in use, after a destructive question) and the policy's
   two values. Load `frontend-design:frontend-design` first.

Never delete under `BetterPullReview-agent-workspaces-v1/` by hand; the
existing 113 GB is reclaimed by this policy on first start.

## Acceptance

- AC1. With retained workspaces older than N days, beyond the cap, or of an
  ended conversation, a profile start removes exactly those, oldest first, and
  never one a turn holds. (Course over the real store and a fixture Git.)
- AC2. Settings shows their count and total size; Remove removes every one not
  in use and the row says what is left.
- AC3. A removed workspace's conversation continues with a fresh workspace.
- AC4. `architecture_boundaries` and `python3 scripts/verify_documentation.py --root .` pass.

## Fix

- **Core.** `choose_expired_workspaces` (`bpr/core/agent_workspace_expiry.hpp`):
  ended, then unused past `KeepDays` (default 7, 0 keeps), then the least
  recently used while the total of all (in-use ones count and stay) exceeds
  `CapGb` (default 20 GB); `choose_all_idle_workspaces` for Remove.
- **Catalog.** `native_agent_session_store_port::retained_workspaces` lists
  each retained workspace once, with its pull request keys, its last use (new
  `native_agent_workspace_use`, stamped whenever a turn stores a conversation
  holding it) and whether any selected conversation of its pull request is at
  its base and head. `forget_workspace` clears it from every conversation,
  their keys and sparse lists in one transaction, bumping their revisions.
- **Files.** `make_installed_retained_workspace_files` (Git adapter) measures
  with a walk that never follows a link, and removes through the exact
  journaled cleanup: the cache's own owner for one this process made (the
  profile now shares `installed_workspace_owners` with the cache), otherwise
  the record's exclusive lock (busy is in use), only for a phase 7 record that
  names the folder, identity and marker. No cache actor is needed.
- **Sweep.** `sweep_retained_workspaces` (application) takes every lease of a
  workspace (each key and a new path lease, which a reattaching turn and the
  workspace check now take too) to tell in use, decides through the core,
  re-takes them around each removal, then forgets it.
- **When.** The profile runs it on its own lane after startup recovery, once a
  day, when a history choice replaces a conversation, and when the policy
  changes; a quit stops it between workspaces.
- **Settings › Agents.** "Agent workspaces limit" (GB), "Remove agent
  workspaces unused for" (days), and "Retained agent workspaces: 3, 17.2 GB"
  with **Remove…**, which asks a destructive question first.
- `docs/architecture/ai-agent-boundary.md` says so (it said there was no
  automatic reclamation, from BPR-1972; this is the owner's BPR-2288).

## Decisions

- "Ended": no selected conversation of the workspace's pull request is at its
  base and head. Clear session at the same revision keeps the workspace (the
  new generation shares it); moving on to another revision ends it.
- Last use is the later of the last stored turn and the newest file in it.
- Remove's question and the line use the frontend-design pass: the scratch
  rows' order, words and quiet button, no new styling.

## Not verified

- No composed (offscreen app) course runs the profile-start sweep; the course
  drives the same `sweep_retained_workspaces` the profile posts. Nothing ran
  against the owner's profile; the 113 GB is reclaimed by the first start of a
  build with this change.
- Journals with no catalog row (an orphan phase 7 record) are not reclaimed.
- `retained_workspace_expiry` is registered on Unix only; Windows is not run.
- No attended look at the Settings rows.
