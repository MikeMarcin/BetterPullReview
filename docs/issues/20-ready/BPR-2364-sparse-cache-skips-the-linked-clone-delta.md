---
schema: "implement-rfc.card.v1"
id: "BPR-2364"
title: "A sparse review whose loading is not recorded as sparse is taken as complete: the agent's workspace skips the fetch of what the linked clone lacks and fails on the cache's missing files, and a whole-history fetch with no haves runs for a minute and times out"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind": "defect", "found_by": "owner, 2026-11-23, build 92 on macOS, on a large repository (the linked clone's packs are 9.4 GiB) whose pull request's head was pushed minutes earlier and was not yet in the linked clone: the first question stopped with 'The agent's copy of this pull request could not be made' and Git's 'fatal: unable to read <blob>'. The review's cache held the head's commit and all its trees but none of its blobs or parents; the workspace logged sparse=0", "related_cards": ["BPR-1772", "BPR-1873", "BPR-1884", "BPR-2265", "BPR-2288", "BPR-2290", "BPR-2291", "BPR-2295"], "related_rfcs": ["RFC-BPR-0092"], "reservation": "docs/issues/_reservations/BPR-2364--BPR-2364--420f6d097cc6.json"}
acceptance: []
---

# BPR-2364: A sparse review whose loading is not recorded as sparse is taken as complete: the agent's workspace skips the fetch of what the linked clone lacks, and a whole-history fetch runs and times out

For a sparse review, a linked clone that lacks the head is not a failure:
[[BPR-1873]] fetches only what the clone lacks into a private store, and the
workspace reads the rest through the clone. That path runs only when the
review is taken as sparse. [[BPR-2295]] decides that from how the review was
*recorded* as loaded (`LoadedSparse`), not from what the cache holds. Its own
Change 1 asked for the second ("Decide this from the cache itself … not from
the transport's state"). When the record says complete, a sparse cache is
treated as a full one and every step after it goes wrong.

## Observed

The owner, 2026-11-23, build 92 on macOS. A large repository, read as
selected objects. A linked clone is configured: a full clone (not shallow,
not partial), with 9.4 GiB of packs. The pull request's head was pushed
minutes before and was not in the clone.

In the Agent tab, the first question stopped:

```text
<agent> stopped before answering
The agent's copy of this pull request could not be made: the review holds only
the pull request's head, and the linked clone was not used. Link your clone in
Settings › Repositories, or check that it holds this head. Your message is kept.

It wrote:
fatal: unable to read <blob of the head>

Stopped after 0:36.
```

The clone *was* linked and was used; it lacked the head, which is what
the delta is for.

The session log, times relative to the review's read:

| Time | Event | Detail |
|---|---|---|
| +0 s | `epoch` `cache_open` … `smart_acquire` | succeeded |
| +0 s → +12 s | `epoch` `protocol_acquire` | succeeded, 12 s |
| +12 s → +15 s | `epoch` `revision_prepare` | succeeded |
| +17 s | `epoch.started` `historical_acquire` | |
| +46 s | `agent.workspace.started` | `workspace_materialize` |
| +79 s | `epoch.failed` `historical_acquire` | `git_step=clone`, `failure=timeout`, `mode=complete_closure source=host linked_clone=0 haves=0 elapsed_ms=61245` |
| +83 s | `agent.workspace.failed` | `failure=history_missing`, 36493 ms, `source=cache_copy_shallow sparse=0 clone=1 clone_head=0 git=pack_objects git_error=missing_object` |

What the review's cache held afterwards (`git cat-file` on its objects
folder, read only):

| Object | In the cache |
|---|---|
| the head's commit | yes |
| the head's trees | all 95,559 |
| the head's blobs | **none of 81,341** |
| the head's first parent | no |

That is a sparse cache ([[BPR-1873]]: the head's commit and trees, no blobs,
no parents). The workspace logged `sparse=0`. The missing blob was in
neither the cache nor the clone.

Running `git fetch origin pull/<n>/head` in the linked clone and resending
works around it.

## Cause

### 1. Sparse is decided from a record, not from the cache

`epoch_application_host::implementation::sparse_cache`
(`src/application/src/epoch_host_review_reads.cpp`, around line 400):

```cpp
return object_loading_for(aContext.Handoff.Shape) == pull_request_object_loading::selected_objects
    && aContext.LoadedSparse.value_or(true);
```

`LoadedSparse` is set once, when the review's context is made
(`src/application/src/epoch_host_materialization.cpp`, around line 1517), to
`selected_objects && SmartGit->supports_sparse_commit_trees()`, and only when
the lease is live. So the review is taken as complete when either:

- `object_loading_for` answers `complete` for this shape
  (`src/application/src/github_window_read_host.cpp`, around line 24: no
  `ReportedCommitCount`, too many files or commits, a count mismatch, and
  `RepositoryBytes` absent or under `maximum_complete_closure_repository_bytes`); or
- the lease did not support sparse trees at that moment.
  `supports_sparse_commit_trees` is false while the provider's `Mismatch` is
  set (`src/adapters/github/src/github_provider.cpp`, around line 1281).

Either way, a cache filled sparse by an earlier read (this session's or a
previous one's: the cache persists) is then read as complete. If its head
commit is already present, nothing is fetched to make it complete. That fits
the 12 s `protocol_acquire` against a 9.4 GiB repository.

Which of the two held here is **not known**. The Standard log records
neither the shape's loading mode nor `LoadedSparse`.

### 2. The workspace takes the full-review path

With `sparse` false (`epoch_host_materialization.cpp`, around lines 728–777),
the workspace skips the head-from-clone attempt, the managed-clone refresh
and the delta ([[BPR-1772]], [[BPR-1884]], [[BPR-1873]]). It calls
`materialize_on_actor` with no delta. The clone lacks the head, so the
cache copy (`src/adapters/git/src/installed_git_process.cpp`, around line
5610) runs:

1. `rev-list` of the head's history fails on the missing parent;
2. [[BPR-2295]]'s fallback writes `shallow` at the head
   (`cache_copy_shallow`);
3. `pack-objects --revs` then needs the head's blobs, which the cache never
   had: `unable to read <blob>`.

The ending maps to `history_missing`, whose words blame the clone link.

### 3. The historical read fetches all history

The same `sparse` false makes the historical read ask for a
`complete_closure` (`src/application/src/epoch_host_dispatch_and_imports.cpp`,
around lines 1295–1335):

- with `HistoryClone` from `clones->lookup`, which logged `linked_clone=0`;
- and with haves only for a base or head whose residency is
  `complete_closure`, so `haves=0` on a sparse cache.

That asks the server for the repository's entire history. It timed out after
61 s; on a faster line it would download all of it, the outcome
[[BPR-2288]] and [[BPR-2291]] exist to prevent.

## Change

### 1. Decide sparse from what the cache holds

- A review's cache is complete for a head only if it holds the head's
  history: the head's first parent, or a recorded `complete_closure`
  residency for that head (`inspect_commit_object`). Otherwise it is sparse,
  whatever `LoadedSparse` and the shape say.
- `sparse_cache` uses this. `LoadedSparse` may still make a review sparse; it
  never makes a sparse cache complete.

### 2. Never fetch whole history without a bound

- A historical or complete read whose haves are empty and whose linked clone
  is not used refuses before it starts on a repository past the
  complete-closure bound, or of unknown size. It falls back to the
  selected-objects read.

### 3. Say what happened

- `history_missing` caused by a clone that lacks the head says so, names the
  head, and says BetterPullReview will fetch what is missing, or why it
  could not.
- Log the loading decision on the review's first read and on each
  workspace: `loading=complete|selected`, `loaded_sparse=0|1|unset`,
  `sparse_capable=0|1`, `cache_has_parent=0|1`.

### 4. Tests

- Application epoch host: a selected-objects review whose `LoadedSparse` is
  false, over a cache holding only the head's commit and trees, with a linked
  clone lacking the head: the workspace fetches the delta and succeeds
  (`source=delta sparse=1 clone_head=0`).
- The same with the shape answering `complete`: the same result.
- A historical read with no haves and no clone on a large or unsized
  repository asks for no complete closure.
- Git process contract: a cache copy is never attempted on a cache that
  lacks the head's blobs.

## Acceptance

- AC1. A review whose cache lacks the head's history is taken as sparse,
  whatever was recorded at load.
- AC2. A sparse review with a linked clone that lacks the head makes its
  workspace from the clone and a fetched delta. Nothing is written to the
  clone.
- AC3. No read asks for a complete closure with no haves on a repository past
  the bound or of unknown size.
- AC4. The log names the loading decision and its inputs.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Owner step

On the large repository: open a pull request pushed after the linked clone's
last fetch and ask a question. The workspace is made without a fetch by
hand, and the log shows `sparse=1` and a delta.

## Out of scope

- Keeping the linked clone up to date (it is the reviewer's own,
  [[BPR-1884]] covers the managed one).
- The 36 s spent before the copy failed: the sibling seed card covers
  workspace speed.
