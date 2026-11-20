---
schema: "implement-rfc.card.v1"
id: "BPR-2295"
title: "An agent workspace for a sparse review is copied from the review's own cache with a full-history walk, which fails at once on the head's missing parent: every question about the pull request stops with 'The agent's copy of this pull request could not be made', and resending, Stop all, a restart and a new session all fail the same way"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind": "defect", "found_by": "owner, 2026-11-19, build 69 on macOS, on a large repository with a linked clone that held the pull request's head and its parents: a question asked from a conversation's comments drawer, 'Was this addressed?', stopped before answering with Git's 'Could not read <parent>' / 'Failed to traverse parents of commit <head>'. Resend, Resume queue, Stop all, an app restart and a new session in the Agent tab failed the same way, five times, each in about 0.1 s", "related_cards": ["BPR-1751", "BPR-1772", "BPR-1873", "BPR-1884", "BPR-1940", "BPR-2246", "BPR-2265"], "related_rfcs": ["RFC-BPR-0092", "RFC-BPR-0103"], "reservation": "docs/issues/_reservations/BPR-2295--BPR-2295--fbb75e53c6b3.json", "amended_by_owner": "2026-11-19: the stopped block is dismissed with the ready box's close (\"The little x in the top right that the agent is ready box has is good enough\")"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "bd3bf8afe", "evidence": {"kind": "command", "detail": "application_epoch_host: a sparse review whose lease no longer loads selected objects (as after a reread's revoke and renewal) still asks the clone for the head and borrows it, with no delta, and logs source=borrowed sparse=1 clone=1 clone_head=1. Every entry point's request is built by dispatch_agent_invocation with linked_history_clone(false); launch_returning (returning questions) no longer drops it; only a tour's tool-free writing passes none. No per-entry-point composed course was added."}}, {"criterion": "AC2", "outcome": "pass", "revision": "bd3bf8afe", "evidence": {"kind": "command", "detail": "git_process_contract (Git 2.50.1): a sparse cache (head commit and trees only) with no clone never stops on the traverse error: the walk fails, the copy ends history at the head (cache_copy_shallow) and refuses on the missing files; a cache with the head's files but not its parent makes a workspace whose history is the head alone. application_epoch_host: a sparse review whose lease no longer loads selected objects gets the pull request's files, never a cache copy; a traverse failure maps to history_missing."}}, {"criterion": "AC3", "outcome": "pass", "revision": "bd3bf8afe", "evidence": {"kind": "command", "detail": "core_operation_diagnosis: workspace_materialize lines carry source, sparse, clone, clone_head, git (command) and git_error (Git's words as a fixed class: failed_to_traverse_parents, could_not_read, missing_object, other) and read back; application_epoch_host: a forced traverse failure logs failure=history_missing source=cache_copy ... git=pack_objects git_error=failed_to_traverse_parents. Raw stderr stays on the turn: the BPR-1828 log policy admits no object name or path (quill_logging_content_policy passes)."}}, {"criterion": "AC4", "outcome": "pass", "revision": "bd3bf8afe", "evidence": {"kind": "command", "detail": "qt_agent_launch_panel: a history_missing stop says the review holds only the head and the clone was not used, offers no resend, and is the only final stop; the Agent tab hides Resend for it and a queue paused behind it says 'Paused: waiting on the stop above...'. Owner addition (2026-11-19): qt workbench case test_a_failed_answer_asks_again_and_says_when_fixed: every stopped block carries the ready box's close (named Dismiss, keyboard focusable), which removes the block and leaves the turn and the composer."}}, {"criterion": "AC5", "outcome": "pass", "revision": "bd3bf8afe", "evidence": {"kind": "command", "detail": "Dev preset, macOS, in a clean worktree of main plus this change: core_operation_diagnosis, application_epoch_host, git_process_contract, qt_agent_launch_panel, quill_logging, quill_logging_content_policy, quill_logging_backpressure, the workbench failed-answer case, architecture_boundaries passed; changed C++ formatted with clang-format 22; verify_documentation DOCUMENTATION_OK."}}]
---

# BPR-2295: An agent workspace for a sparse review is copied from the review's own cache with a full-history walk, which fails at once on the head's missing parent

BPR-2288 is the card *On a large repository, opening a file whose base the
cache lacks runs a full clone … and every retained agent workspace copies all
of history*.

## Observed

The owner, 2026-11-19, build 69 on macOS, with a custom agent of the Claude
stream JSON protocol. The repository is large, and its pull requests are read
as selected objects (a sparse review). A linked clone is configured. It is not
the clone BetterPullReview keeps, and it is not shallow.

1. In a conversation's **comments drawer** the owner asked "Was this
   addressed?". The Agent tab showed the question, then:

   ```text
   <agent> stopped before answering
   The agent's copy of this pull request could not be made. Resend to try again, or show the log for the cause. Your message is kept.
   ```

   The turn's block quoted Git:

   ```text
   error: Could not read <parent of the head>
   fatal: Failed to traverse parents of commit <head>
   ```

   It ended with "Stopped after 0:00.". The question stayed under
   **Waiting**, with **Resume queue**.
2. **Resume queue**, **Resend**, **Stop all**, quitting and reopening the app,
   and **New session** in the tab all failed the same way. To the owner it
   looked as if the question was stuck in a queue and would never be asked.
3. **Check agent** for the same pull request, in the same app session,
   succeeded. It made and removed its workspace in 27 s.

The session log, after the restart:

| Time | Event | Detail |
|---|---|---|
| +0 s | `workspace.clone_scan.succeeded` | `clone_scan`, `count=10` |
| +2 s | `agent.workspace.failed` | `workspace_materialize`, `copy_failed`, 0 ms |
| +11 s | `agent.workspace.succeeded` | `workspace_materialize`, 27271 ms (the Check) |
| +16 s | `agent.workspace.succeeded` | `workspace_cleanup`, 5218 ms (the Check) |
| +2 min | `agent.workspace.failed` | `workspace_materialize`, `copy_failed`, 117 ms |
| +2 min | `agent.workspace.failed` | `copy_failed`, 106 ms |
| +2 min | `agent.workspace.failed` | `copy_failed`, 96 ms |
| +5 min | `agent.workspace.failed` | `copy_failed`, 130 ms |

No `agent.run.started` follows any of them. The log holds neither Git's
message nor which source the workspace was being copied from.

What was checked afterwards, by hand:

- The linked clone held the head and its parent. Its remote-tracking branch
  had been fast-forwarded to the head more than 20 minutes before the first
  failure.
- A fresh repository that borrows the clone's objects folder through
  `objects/info/alternates`, as a workspace does, reads both commits, and
  `pack-objects --revs` from the head walks all its history without error.
  This held for both Git builds on the machine.

So the failing copy did not read through the clone. It read the review's own
cache. A sparse review's cache holds the head's commit and trees, but none of
its parents or blobs ([[BPR-1873]]).

## Cause

### 1. One branch copies a workspace out of the cache with `--revs`

`src/adapters/git/src/installed_git_process.cpp` picks how a workspace gets
its head:

| Branch (around line) | When | How |
|---|---|---|
| Pull request files (~5080) | `Source.PullRequestFiles` | the head's trees from the cache, the pull request's blobs, sparse checkout, shallow at the head |
| Borrowed (~5166) | `borrowed && present(head)` | nothing to copy; the clone lends it |
| Head from clone (~5168) | `HeadFromClone` | refused: "The linked clone does not hold the head." |
| Delta (~5175) | `Source.Delta` | index the fetched delta; read the rest through the clone |
| **Cache copy (~5222)** | **anything else** | **`pack-objects --revs` from the head (and the base), with the cache as an alternate** |

The cache copy walks every ancestor of the head. That is right for a full
review, whose cache holds all of history. On a sparse review's cache it fails
on the first parent, which is exactly the error the owner saw. It writes no
`shallow` file and has no fallback. The retained copy step's fallback
([[BPR-2246]], around line 5355) does not apply, because that step runs only
when `borrowed`.

The error can't have come from the retained copy step either. That step runs
only while borrowing the clone, which walks cleanly, and when it fails it
retries with the history ending at the head. A walk ending at the head can't
fail on the head's parents.

### 2. A sparse review reaches the cache copy

`epoch_application_host::implementation` decides the source
(`src/application/src/epoch_host_materialization.cpp`, around lines 716–768):

- `sparse = context && blob_authority_for(*context) == authorization && sparse_review(*context)`.
- `sparse && !HistoryClone` → pull request files.
- `sparse` with a clone → head from clone, then the managed-clone refresh,
  then the delta.
- **Not `sparse`** → `materialize_on_actor` with no delta and no
  `HeadFromClone`. That ends at the borrowed branch if a clone holds the head,
  and otherwise at the cache copy.

So the cache copy is reached for a sparse cache when both hold:

- **`sparse` answered false for a sparse cache.** `sparse_review`
  (`epoch_host_review_reads.cpp`, around line 391) also requires
  `!aContext.Handoff.SmartGit->revoked()`, and `BlobContext` is a weak
  reference that may be missing or belong to an older authority. A revoked
  or replaced read transport, a missing context or an authority mismatch
  therefore makes a sparse review look full. Which of these happened here is
  not known. A revoked transport kept past a renewal, as in [[BPR-2265]], is
  the leading candidate.
- **The borrowed branch was not taken.** Either the request had no
  `HistoryClone`, or `present(head)` answered false through it. The clone
  holds the head and borrows cleanly by hand, so a missing `HistoryClone` is
  more likely. The drawer's question and the Agent tab both take
  `linked_history_clone(aWritingOnly)`
  (`src/app/src/github_application/window_agent_chat.cpp`, around line 832),
  and a tour writing run takes it only when read-only (around line 868).
  Which of these the drawer's question used is not known. Check it works
  passes `linked_history_clone(false)` and never runs the cache copy's walk,
  which is why it succeeded.

The raw stderr and the chosen branch were not logged, so this section names
the two conditions the code requires, not a proven trace.

### 3. The failure looks permanent and says nothing useful

- The failure is deterministic, so Resend and Resume queue repeat it. A
  restart doesn't change it either, if the conditions come back on the first
  read.
- The page says "could not be made … show the log for the cause". The log has
  only `copy_failed` (`agent_invocation_host.cpp`, around line 975 keeps Git's
  text as `StandardError` for the turn, not for the log).
- The queued question waits behind a run that can never start, with no word
  that the queue is blocked by that failure.

## Change

### 1. Never walk history in a cache that doesn't hold it

- The cache copy branch first checks whether the cache can supply the
  head's history: a full review's cache, not a sparse one. Decide this from
  the cache itself (for example, `cat-file -e <head>^1` or the cache's
  recorded loading mode), not from the transport's state.
- If it can't, the cache copy isn't used. In order:
  1. borrow a linked clone that holds the head;
  2. take the sparse path ([[BPR-1772]], [[BPR-1873]]: head from clone,
     managed-clone refresh, delta);
  3. with no clone, the pull request files path.
- If the cache copy is used and a parent turns out missing anyway, end the
  workspace's history at the head and the base (write `shallow`, as the
  retained copy's fallback does) instead of failing the turn.

### 2. Decide "sparse" from the review, not from a live transport

- Whether the review is sparse is a fact about how it was loaded. Record it
  with the review's authority and cache, and read it from there. A revoked or
  replaced read transport may stop a fetch, but it must not make a sparse
  cache look full.
- If `BlobContext` is missing or belongs to another authority, wait for or
  rebuild the current one, or refuse with its own reason. Don't fall through
  to the full-cache path.

### 3. Every entry point passes the linked clone

- The comments drawer, the Agent tab, the comment workshop and the
  returning questions all pass `linked_history_clone(false)` when the run may
  read files. Only tool-free writing passes none. Test it per entry point.

### 4. Log the source and Git's words

- `workspace_materialize` logs which branch it took (`pull_request_files`,
  `borrowed`, `delta`, `cache_copy`), whether the review was sparse, whether
  a clone was given and whether it held the head.
- On failure, log Git's command name and the first lines of its stderr,
  sanitized and bounded, and a typed reason: `history_missing` for a
  parent-walk failure, instead of the generic `copy_failed`.

### 5. Say what failed, and don't retry what can't succeed

- A `history_missing` failure says: "The agent's copy of this pull request
  could not be made: the review holds only the pull request's head, and the
  linked clone was not used. Your message is kept." Point to the clone
  setting when no clone is linked.
- With a deterministic failure, **Resume queue** and **Resend** are not
  offered as the fix. The queued question says it is waiting on that failure.

## Tests

- Git adapter: a sparse cache (head commit and trees only) with no clone and
  no delta never runs `pack-objects --revs` on it. With a parent missing, the
  cache copy ends history at the head and succeeds, or refuses as
  `history_missing`.
- Epoch host: a sparse review whose read transport is revoked, or whose
  `BlobContext` is missing, still takes the sparse path, borrows the clone
  that holds the head, and asks the host for nothing.
- App composition (fixture course): a question from the comments drawer on a
  sparse review with a linked clone that holds the head is answered. The log
  shows `borrowed`.
- The same after a reread that renews the read transport.
- Diagnostics: a forced parent-walk failure logs `history_missing`, the
  branch and Git's first stderr line.

## Acceptance

- AC1. On a sparse review with a linked clone that holds the head, every
  entry point (Agent tab, comments drawer, comment workshop, returning
  questions) makes its workspace by borrowing the clone, after any number of
  rereads.
- AC2. No workspace is made by walking history in a cache that doesn't hold
  it. A missing ancestor never fails a turn in under a second with Git's
  traverse error.
- AC3. A workspace failure's log says which source was used, whether the
  review was sparse, whether a clone was given, and Git's own words.
- AC4. A deterministic failure is not offered for blind retry, and a queued
  question says what it is waiting on.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## What was found (2026-11-19)

- **The missing clone is proven.** "Was this addressed?" is a returning
  question (`evaluate_proposed_fix`, RFC-BPR-0116). For an agent of the
  Claude stream JSON protocol, the owner's custom agent among them,
  `launch_returning` (`window_returning_review.cpp`) made the first answer
  tool-free and dropped `HistoryClone`. The workspace was still made, from the
  cache. Check it works passes the clone, which is why it succeeded.
- **Sparse read as complete.** The pull-request-files path never walks
  history, so `sparse` answered false too. `sparse_review` needs the lease
  unrevoked and `supports_sparse_commit_trees()`, and the lease answers false
  to both once revoked. Which event revoked it here is not proven; the
  decision no longer depends on it.
- **BPR-2291 widens the exposure.** `object_loading_for` now loads every
  repository past 32 MiB with selected objects (a lease without sparse
  support is refused). Every large repository's review therefore has a
  sparse cache, and any of them could reach this failure. Before BPR-2291 a
  large repository whose pull request exceeded the count limits loaded
  complete and was not exposed.

## Delivered

- The review records how its cache was loaded (`LoadedSparse`) while its
  lease is live, and `sparse_cache` reads that, not the lease. A review loaded
  with selected objects and nothing recorded is taken as sparse. A missing or
  other-authority context is refused as `stale`.
- A cache copy first walks the head's commits (`rev-list --quiet`). When the
  walk fails, the copy's history ends at the head and the base the cache
  holds (the `shallow` file, kept with any clone boundary). A cache that also
  lacks the head's files refuses as `history_missing`.
- Returning questions keep the linked clone. Only a tour's tool-free writing
  passes none.
- `workspace_materialize` lines carry `source=`, `sparse=`, `clone=`,
  `clone_head=`, `git=` and `git_error=`. Git's words are logged as a fixed
  class (`failed_to_traverse_parents`, `could_not_read`, `missing_object`,
  `other`) because the BPR-1828 log policy admits no object names or paths.
  The raw text stays on the turn.
- A `history_missing` stop names the cause and offers no Resend. A queue
  paused behind it says it waits on that stop. Every "stopped before
  answering" block has the Agent ready box's × (owner, 2026-11-19: "The little
  x in the top right that the agent is ready box has is good enough"), which
  removes the block and leaves the message.

## Out of scope

- How much history a retained workspace copies, and fetching missing commits
  shallow (BPR-2288). That card changes the retained copy step and the
  history fetch. This one changes which branch is taken and the cache copy
  branch. Both can land in either order.
- Revoked mutation leases after a reread ([[BPR-2265]]). If Cause 2 turns out
  to be the same renewal, fix it at the shared seam, but this card's guard
  (Change 1) holds either way.
