---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AO"
title: "An agent question on a sparse review stops at workspace_unavailable: preparing its workspace walks into the head's missing parent"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner report, 2026-10-22: an external terminal harness asked about selected lines of a pull request read over the gh transport","related_cards":["BPR-0341","BPR-0348","BPR-1377","BPR-1687"],"related_rfcs":["RFC-BPR-0011","RFC-BPR-0013","RFC-BPR-0089"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"77b44c502c4b33d25929c01991a3da99ddab7a6e","evidence":{"kind":"command","detail":"test_better_pull_review_git_process_contract_tests: run_sparse_agent_workspace_contract imports a real sparse pack (head commit and trees, parent absent), shows the old copy fails with Git's 'Failed to traverse parents', and makes a working workspace from a head-only snapshot whose history ends at the head."}},{"criterion":"AC2","outcome":"pass","revision":"77b44c502c4b33d25929c01991a3da99ddab7a6e","evidence":{"kind":"command","detail":"test_better_pull_review_application_epoch_host_tests: sparse_agent_workspace_is_made_from_the_heads_snapshot fails before the fix (the fake sparse cache refuses a copy without a snapshot, as Git does) and passes after: one snapshot request for the session's head through the sparse lease, handed to the cache; complete reviews unchanged; an unheld head is never fetched."}},{"criterion":"AC3","outcome":"pass","revision":"77b44c502c4b33d25929c01991a3da99ddab7a6e","evidence":{"kind":"command","detail":"test_better_pull_review_gh_cli_git_smart_transport_tests: the snapshot clone takes --depth=1 without a blob filter; its owner-private pack holds the head, its trees and blobs and no ancestor; complete reviews are refused; the pack goes with the snapshot."}},{"criterion":"AC4","outcome":"pass","revision":"77b44c502c4b33d25929c01991a3da99ddab7a6e","evidence":{"kind":"command","detail":"Transport failures map to closed reasons (connection, sign-in, too large, download, copy); agent_invocation_host carries the reason with the outcome; qt_agent_launch_panel shows the plain reason and recovery while the log keeps Git's text and the status keeps 'Finished: workspace_unavailable'."}}]
---

# BPR-PENDING-AO: An agent question on a sparse review stops at workspace_unavailable: preparing its workspace walks into the head's missing parent

## Observed

On macOS, a pull request opened over the gh transport, the Agent tab's
external terminal harness asked about selected lines. Nothing was launched.
The banner read "External terminal harness stopped before answering —
Finished: workspace_unavailable. Your question is kept." and its log held
Git's own text:

```
error: Could not read <parent of the head>
fatal: Failed to traverse parents of commit <head>
```

The commit identities are the owner's and are not recorded here.

## Cause

A review read over the gh transport loads selected objects only
(`object_loading_for` answers `selected_objects` for every bounded pull
request shape, and the gh lease supports sparse commit trees). Its private
cache therefore holds each epoch's commit and trees, imported from a
`--depth=1 --filter=blob:none` clone: no parent and no blob
([[BPR-1377]] counted six objects and no blobs).

A disposable agent workspace was copied out of that cache by
`installed_git_process::materialize_agent_workspace`, which ran
`git pack-objects --revs` with the head on standard input. `--revs` packs the
head's whole reachable closure, so Git walked to the head's first parent,
which the sparse cache never held, and failed with the text above. The
host mapped it to `workspace_unavailable` ([[BPR-0348]]) and the Agent tab
showed the outcome's stable name and Git's log. Had the walk not failed, the
next missing object would have been the first blob: checking out the head
needs every file, and the sparse cache holds none.

RFC-BPR-0089's chain-tree acquisition ([[BPR-1687]]) fetches trees for the
commit filter and is not used for the workspace.

## Fix

The smaller correct fix is to fetch exactly what the workspace needs, not to
deepen the review's clone:

- **Transport.** `git_smart_transport_port::acquire_workspace_snapshot`
  asks for one commit's snapshot. The gh transport clones that commit with
  `--depth=1` and no blob filter, under the same qualification, account
  observations before and after, and 32 MiB file / 96 MiB directory bounds as
  the review's own sparse acquisition, writes the commit as the store's
  shallow boundary, and packs it with `pack-objects --revs`. The pack holds the
  commit, its trees and its blobs and no ancestor. It is owner-private and is
  removed with the snapshot.
- **Host.** `epoch_application_host::materialize_agent_workspace` asks for the
  snapshot only for a sparse review (the same predicate as the chain-tree
  acquisition), off the actor and through the review's own lease, and only
  for a commit the review's cache proves it holds. It hands the pack to the
  cache and holds the snapshot until the copy is done. A complete review is
  unchanged: its workspace is still copied out of the cache, with history.
- **Git adapter.** Given a snapshot, the workspace indexes that pack
  (`index-pack --stdin` with the sparse fsck policy) instead of packing the
  cache's closure, and records the head in `.git/shallow`, so Git inside the
  workspace treats the head as having no parent instead of failing.
- **Message.** `agent_workspace_error` and `agent_run_result` carry a closed
  `agent_workspace_unavailable_reason`. The Agent tab's stopped banner says
  what happened and what to do; Git's or the transport's own words stay behind
  "Show the log", and the panel status keeps `Finished: workspace_unavailable`.

| Reason | Banner text |
|---|---|
| `copy_failed` | The agent's copy of this pull request could not be made. Reopen the pull request, then ask again. |
| `download_failed` | The agent needs every file at this head, and this review holds only the files it shows. Downloading the rest failed. Ask again, or show the log for the cause. |
| `connection_failed` | The agent needs every file at this head, and GitHub could not be reached to download them. Check the connection, then ask again. |
| `sign_in_refused` | The agent needs every file at this head, and GitHub refused to send them to the signed-in account. Check that gh is signed in with access to this repository, then ask again. |
| `too_large` | The agent needs every file at this head, and this head is larger than the agent's private copy allows. |

## What the sparse copy supports

The workspace made this way holds every file at the head and one commit.

- It supports reading and searching the head's files, building and running
  tests that need only the tree, and `git status`, `git show HEAD` and
  `git ls-files`.
- It cannot support history: `git log` shows one commit, `git blame`
  attributes every line to the head, and the base commit is not present, so
  `git diff <base>` fails. The base and head identities are still in the
  agent's context.
- It is bounded by the gh transport's 32 MiB packed snapshot, and a larger
  head fails as `too_large`.

The owner's intended workflow, a real local clone with a dedicated worktree at
the pull request's head, belongs to the Agent-setup design and is not built
here.

## Acceptance

- AC1. A sparse cache whose head's parent is absent yields a working
  workspace with every file at the head, and Git inside it does not fail on the
  missing parent.
- AC2. A sparse review's workspace is made from the head's snapshot fetched
  through the review's own lease, and a complete review's workspace is
  unchanged.
- AC3. The gh transport's snapshot holds the head, its trees and its blobs and
  no ancestor, is owner-private and bounded, and is removed when released.
- AC4. A workspace that cannot be made tells the reviewer why and what to do,
  and keeps the raw text in the log.

## Verification

Run on macOS, with a lean `-g0` dev build at the candidate and Git 2.50. All
of these passed:

- `application_epoch_host`, `application_agent_invocation_host`,
  `application_agent_workspace`, `application_private_review_host`
- `qt_agent_launch_panel`, offscreen
- `git_process_contract`, `git_cache_contract`, `git_sparse_cache_contract`
- `gh_cli_git_smart_transport`
- the composed `application_agent_invocation_composition_contract`, offscreen
- `architecture_boundaries` and `coding_format_check` (clang-format 22)
- clang-tidy on the changed translation units

As a before-fix control, with the host's snapshot branch disabled,
`application_epoch_host` failed at "a sparse review's agent workspace is
made, not refused at the head's first missing parent". The git process
contract reproduces the reported text with real Git through the old copy.

The documentation gate passes except for refusing the provisional ID's
format, which allocation resolves. No live GitHub course was run, because
the owner's repository and commits are private and none was needed: the
transport contract drives the real clone, pack and index commands against a
local repository.
