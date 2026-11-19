---
schema: "implement-rfc.card.v1"
id: "BPR-2289"
title: "A retained agent workspace keeps borrowing the linked clone's history instead of packing all of it, and repairs itself from the review's cache when the clone lost the head or base"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2288--BPR-2291--19d6a88ae1a3.json","parent":"BPR-2288","origin":"owner card BPR-2288 (2026-11-19), Change 5 option 1","related_cards":["BPR-1751","BPR-2246","BPR-2288","BPR-2291"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"888a8ff9f","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^git_process_contract$'","detail":"run_borrowed_history_workspace_contract (retained): the workspace keeps one alternates line naming the clone's objects, BorrowsHistory is set, its own pack holds 3 objects (the head's commit, tree and new blob the clone lacks), the clone's file inventory is unchanged, and a reattach while the clone lends head and base keeps the line."}},{"criterion":"AC2","outcome":"pass","revision":"888a8ff9f","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^git_process_contract$'","detail":"Same course: the clone is moved away and a fresh adapter reattaches. With no valid cache to repair from the reattach refuses as object_unavailable and leaves the alternates line and shallow list as they were; with the review's cache it copies the head and base, removes the line, writes .git/shallow = head, base, keeps the agent's tracked and untracked edits, and resumes again with nothing to copy."}},{"criterion":"AC3","outcome":"pass","revision":"888a8ff9f","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(git_process_contract|agent_workspace_ownership|application_agent_workspace|application_agent_invocation_host)$'","detail":"BPR-2246's run_incomplete_clone_history_workspace_contract still passes with the line kept: a clone shallow at mid gives history 2 deep with no base required, a depth-one clone gives the head, a clone missing an ancestor gives a workspace whose history ends at head and base and requires the base."}}]
---

# BPR-2289: A retained agent workspace keeps borrowing the linked clone's history instead of packing all of it

Part of [[BPR-2288]] (Change 5, option 1).

## Cause

A native Agent-tab workspace (`RetainForSession`) borrowed the linked clone
through one alternates line, then ran `pack-objects --revs` over the head and
base and removed the line, so it would not depend on a clone that can move or
gc ([[BPR-2246]], RFC-BPR-0096). On a large repository that pack is the whole
history: 6.9-7.3 GB and minutes per workspace, 113 GB over 13 workspaces.

## Fix

- `materialize_agent_workspace`: a retained workspace keeps the alternates
  line, as a disposable one does ([[BPR-1751]]). Its own pack is only what the
  clone lacks of the head. A commits-only walk (`rev-list --quiet`, which uses
  the clone's commit-graph) replaces the object walk; when it fails on a missing
  ancestor the workspace's history ends at the head and the base, as BPR-2246
  did.
- `inspect_agent_workspace` (reattach) now takes the review's cache path. It
  accepts exactly one alternates line naming an objects folder outside the
  workspace (present, or gone). When the borrowed store no longer lends the
  head or the required base, it copies them from the cache (read only), writes
  `.git/shallow` at them and removes the line; if the cache cannot supply
  them, it puts the line and shallow list back and refuses as before
  (`retained_objects_missing`).
- `docs/architecture/ai-agent-boundary.md` says so.

Workspaces made before this change keep their own pack and no line, and reattach
as before. The 113 GB already on disk is not touched; expiry is [[BPR-2291]].

## Decision

Change 5 option 1 (keep the alternates entry, verify on reattach), the owner's
first preference. "Fetch exactly the missing commits" is done locally from the
review's cache, which already holds the head and base of a complete review; a
sparse review's cache has no blobs, so such a workspace whose clone is gone is
refused with the existing words. Re-borrowing a moved clone at its new path is
not done (the reattach does not know the link's current path).

## Not verified

- A real large repository: the owner's clone was not used. Ready-in-seconds is
  not claimed: the checkout still writes the head's files (80,733 files there),
  which is [[BPR-2291]].
