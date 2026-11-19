---
schema: "implement-rfc.card.v1"
id: "BPR-2288"
title: "On a large repository, opening a file whose base the cache lacks runs a full clone that times out and restarts, and every retained agent workspace copies all of history: files never load, workspaces take minutes and 13 of them hold 113 GB"
status: "Backlog"
rfcs: []
depends_on: ["BPR-2289", "BPR-2290", "BPR-2291"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2288--BPR-2291--19d6a88ae1a3.json","split_into":["BPR-2289","BPR-2290","BPR-2291"],"found_by":"owner, 2026-11-19, build 69 on macOS, on a large repository (about 7.1 million objects, 9.4 GiB of packs, 80,733 files at the head) with a linked clone: one C++ file and one Markdown file each took minutes to show and showed no reason; a stack sample and the process list found the file view waiting on a full `gh repo clone` that timed out and restarted. The same day, the Agent tab spent minutes 'making its workspace at the head', and the retained agent workspaces were found to hold 113 GB","related_cards":["BPR-1751","BPR-1752","BPR-1772","BPR-1873","BPR-1879","BPR-1940","BPR-2246"],"related_rfcs":["RFC-BPR-0092","RFC-BPR-0108"]}
acceptance: []
---

# BPR-2288: On a large repository, opening a file whose base the cache lacks runs a full clone that times out and restarts, and every retained agent workspace copies all of history: files never load, workspaces take minutes and 13 of them hold 113 GB

Two defects with one root: on a large repository the app copies **all of
history** when it needs one commit, and ignores a linked clone that already
has it. Part A is the file view. Part B is the agent workspace.

## Observed

The owner, 2026-11-19, build 69 on macOS. The repository is large: about 7.1
million objects in 9.4 GiB of packs, and 80,733 files (1.6 GB) at the head.
A linked clone is configured. It is fetched to within hours of the remote's
default branch.

### A. A file takes minutes to show, or never does

The owner opened a pull request whose base is that morning's default-branch
commit, then opened a changed C++ file. The file view stayed loading for
minutes. An earlier pull request did the same with a 70 KB Markdown file.

- **The log ends at the comparison.** The pull request read and the Git
  acquisition finished normally: `protocol_acquire` 8 s, `revision_prepare`
  3 s, the last `catalog_prepare` at 12:56:40. After that the log has nothing
  but an unrelated reread. The step the file waited on writes no event at
  any detail level (Standard or Verbose).
- **A stack sample** (macOS `sample`, 10 s, while the file loaded) showed:
  - the main thread idle in the event loop, so the window stayed responsive;
  - the file view's work blocked in
    `epoch_application_host::prepare` → `prepare_selected_diffs` →
    `resolve_selected_diff`, waiting on a condition;
  - another worker in `acquire_git_protocol_v2_historical_pack` →
    `github::detail::run_gh_cli`, waiting on a child process.
- **The child process** was:

  ```text
  gh repo clone https://<host>/<owner>/<repo>.git /private/tmp/bpr-cli-git-QZuyxb/clone-0.git --no-upstream -- --bare --quiet --revision=<base commit> --template=… --config transfer.unpackLimit=0 --config fetch.unpackLimit=0 --config core.hooksPath=/dev/null --config core.fsmonitor=false
  ```

  - It ran `git clone --bare --revision=<base commit>`, with no `--depth`,
    no `--filter` and no `--reference`.
  - Its `tmp_pack` was still **0 bytes** after more than a minute. The server
    was still counting and compressing the whole repository.
  - A few minutes later that clone and its folder were gone, and a new one
    was running in `/private/tmp/bpr-cli-git-eeXiqN`. The fetch had been
    abandoned and **restarted**.
- **The commit was already local.** The base commit was in the owner's
  linked clone.

### B. The Agent tab takes minutes to make its workspace

- The status read "… is making its workspace at the head, 0:46." and kept
  counting. Earlier sessions logged `workspace_materialize` at 111–137 s.
- During it, the agent supervisor ran
  `git … -C <workspace> pack-objects --revs --delta-base-offset --non-empty -- <workspace>/.git/objects/pack/pack`.
- Each retained workspace holds a private pack of **6.9–7.3 GB** and no
  `objects/info/alternates`. With its checkout, each is **8.6–8.7 GB**.
- There were **13** retained workspaces, the oldest eight days old: **113 GB**
  under `BetterPullReview-agent-workspaces-v1/`.

## Cause

### A. A missing historical commit is fetched as a full clone, with a 2-minute bound

1. **The mode needs a whole closure.** When a comparison needs a commit the
   review cache does not hold, the epoch host builds a smart request with
   `AcquisitionMode = complete_closure`, unless the pull request is loaded as
   selected objects and the transport supports sparse commit trees
   (`src/application/src/epoch_host_dispatch_and_imports.cpp`, around lines
   756–767 and 1260–1268).
2. **`complete_closure` becomes a full clone.**
   `src/adapters/github/src/gh_cli_transport.cpp`, around lines 1753–1786,
   builds:
   - `gh repo clone … -- --bare --quiet --revision=<commit>` plus fixed
     config;
   - `--reference=<linked clone>` **only** on the delta path, when the
     workspace borrows a clone;
   - `--depth` and `--filter=blob:none` **only** for `sparse_commit_trees`.

   So for `complete_closure` with no delta, Git downloads every object
   reachable from the commit: the repository's whole history. On this
   repository that is gigabytes, and the server spends minutes before sending
   a byte.
3. **The bound is two minutes.** The request's `Timeout` is
   `std::chrono::minutes{ 2 }` (same function). A full clone of a large
   repository cannot finish in it. It is abandoned and, as observed, started
   again, so the file can wait for ever.
4. **The linked clone is not asked.** The commit was in the owner's linked
   clone. Outside the delta path, nothing reads it from there, or passes it
   as `--reference`.
5. **Nothing says what is happening.** No diagnostic event covers the
   historical acquisition, its child process, its timeout or its restart.
   The file view shows a plain loading state, with no "fetching from the
   host" and no progress.

### B. A retained workspace re-packs all reachable history

1. A workspace first borrows the linked clone's objects through alternates.
   That costs nothing ([[BPR-1751]],
   `src/adapters/git/src/installed_git_process.cpp`, around lines 5038–5064).
2. For a session-retained workspace (`RetainForSession`, set for native
   Agent-tab runs in `agent_invocation_host.cpp`, around line 968), it then
   runs `pack-objects --revs` over the head and base. It writes everything
   reachable into its own pack, and deletes the alternates entry (around
   lines 5343–5374). On this repository that is about 7 GB, every time.
3. It also writes the whole head tree with `read-tree --reset -u` (around line
   5160): 80,733 files, 1.6 GB, although the linked clone has a working tree
   near that head.
4. Retained workspaces are not expired or shared. One is made per pull
   request base/head binding and kept, so the disk use grows by about 8.7 GB
   per reviewed pull request.

The copy exists so a retained workspace does not depend on a clone that can
change, gc or move ([[BPR-2246]]). That goal holds. Copying all of history is
far more than it needs.

## Change

### 1. Find a missing commit locally first

- Before any network acquisition, look for the commit in the linked clone
  (`cat-file -e <commit>^{commit}`). If it is there, bring only what the
  comparison needs into the review cache, through a local fetch or
  `pack-objects` with the cache's haves, as the delta path already does. Read
  the clone only; write nothing to it ([[BPR-1751]]).
- When the network is needed and a linked clone exists, pass it as
  `--reference` for every acquisition mode, not only the delta path.

### 2. Never ask the host for all of history to get one commit

- For a missing base or head commit, fetch it shallow: `--depth=1` with
  `--filter=blob:none`, then fetch only the blobs of the files the comparison
  shows. Or use a protocol v2 fetch of exactly that commit with the same
  filter, as `sparse_commit_trees` already does for heads.
- Where a comparison really needs history (merge base, blame), fetch it with
  a bound depth, and deepen only on demand.
- `complete_closure` with no depth and no filter is allowed only for a
  repository below a size threshold, or when the reviewer asked for full
  history.

### 3. Bound and retry honestly

- The timeout covers progress, not the whole operation. A fetch that is
  still receiving data isn't killed at two minutes, and one that sends
  nothing for N seconds is.
- A timed-out acquisition is not restarted unchanged. It backs off, or falls
  back to the shallower form (Change 2), or stops and says so (Change 4).
  Never an unbounded loop of identical full clones.
- Cancelled or timed-out temporary clones are removed (they were here). Keep
  that.

### 4. Log it and show it

- Log the historical acquisition as an epoch stage, with:
  - its mode;
  - where the commit came from (cache, linked clone or host);
  - bytes received;
  - duration, timeout and retries;
  - its outcome.
- While a file waits on one, the file view says so: "Fetching the base
  commit from <host>…", with bytes received when known, and a **Cancel**.
  After a timeout it says what failed and what the reviewer can do.

### 5. A retained workspace shares history instead of copying it

Pick one, in this order of preference:

1. **Keep the alternates entry,** and verify on reattach that the borrowed
   store still holds the head and base. If not, re-borrow, or fetch exactly
   the missing commits (Change 2). This is the cheapest option and keeps
   history readable.
2. Otherwise **copy only the head and base commits and their trees,**
   shallow, with blobs only for paths the agent opens. Not
   `pack-objects --revs` over all history.
3. Make the checkout from the linked clone's existing work where possible: a
   worktree, or `read-tree` with the clone's index as a cache. Or check out
   sparsely: the pull request's files and their directories, widened on
   demand.

Expire retained workspaces: after their session ends, after N days unused,
and under a total-size cap, oldest first. Show the space they use in
Settings, with **Remove**.

### 6. Tests

- Epoch host: a comparison whose base is missing from the cache but present
  in the linked clone is prepared with no host request, and the log says
  "linked clone".
- GitHub CLI transport: no `complete_closure` request on a repository above
  the threshold runs `clone` without `--depth` or `--filter`. With a linked
  clone, every request carries `--reference`.
- Timeout: a fetch that keeps receiving data isn't killed at the old bound.
  A stalled one falls back or stops, and never repeats the same full clone.
- Qt: a file waiting on a fetch shows the fetching state, and Cancel stops
  it.
- Workspace: a retained workspace on a repository with a linked clone writes
  no pack larger than its head and base need. Reattach after the clone is
  gc'd re-borrows or fetches only what is missing. Expiry removes the oldest
  workspaces beyond the cap.

## Acceptance

- AC1. Opening a file whose base or head is missing from the review cache,
  while the linked clone has it, sends no request to the host and shows the
  file within seconds.
- AC2. Without a linked clone, the app fetches only the commit and the
  objects the comparison needs, never a clone of the repository's history,
  on a repository of any size.
- AC3. No acquisition repeats the same full clone after a timeout. A stalled
  one ends with a logged reason and a message in the file view.
- AC4. Every historical acquisition is in the diagnostic log with its source,
  size, duration and outcome. A file waiting on one says so.
- AC5. A retained agent workspace on a large repository is ready in seconds
  when a linked clone exists, uses disk proportional to the files it holds
  rather than to the repository's history, and still survives the clone's
  gc (by re-borrowing or fetching).
- AC6. Retained workspaces expire and stay within a size cap, and Settings
  shows their total size.
- AC7. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Making the host faster at building packs.
- How the review cache itself is laid out, beyond not copying all of history
  to fill a gap.
- Removing the 113 GB already on disk. Expiry (Change 5) will, and a one-off
  cleanup on update may be worth a line in the release notes.

## Split

The owner's card is kept as written above. It closes when its three parts do:

- [[BPR-2289]]: Change 5 option 1. A retained workspace keeps borrowing the
  linked clone, and a reattach copies the head and base from the review's cache
  when the clone lost them (AC5 in part).
- [[BPR-2290]]: Changes 1, 3 (no repeated clone) and 4 (the log). A complete
  acquisition reads the linked clone first, borrows it as `--reference`,
  honours the cache's haves, logs `historical_acquire`, and a background import
  that hits a wall stops the rest (AC1 and AC3 in part, AC4's log).
- [[BPR-2291]]: what is left. The file view's fetching state and Cancel, a
  progress-based timeout, a bounded fetch with no linked clone (AC2), the
  retained-workspace expiry, size cap and Settings row (AC6), and the checkout
  cost of a large head (AC5's "ready in seconds").
