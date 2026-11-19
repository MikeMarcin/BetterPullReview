---
schema: "implement-rfc.card.v1"
id: "BPR-2293"
title: "Retained agent workspaces expire after their session ends, after N days unused and under a total-size cap, oldest first, and Settings shows their total size with Remove"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2293--BPR-2294--e7efc87e257e.json","parent":"BPR-2288","origin":"split from BPR-2291 (2026-11-19): BPR-2288 Change 5's expiry and AC6","related_cards":["BPR-2288","BPR-2289","BPR-2291","BPR-2246","BPR-2261"]}
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
