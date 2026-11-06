---
schema: "implement-rfc.card.v1"
id: "BPR-1873"
title: "An agent's workspace comes from a linked or BetterPullReview-managed clone, never a head snapshot download"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05: agent question on a large repository's newly pushed head stopped with 'Downloading the rest failed' (transport failure 1, step 9)","related_cards":["BPR-1738","BPR-1751","BPR-1755","BPR-1772"],"related_rfcs":["RFC-BPR-0092"]}
acceptance: []
---

# BPR-1873: An agent's workspace comes from a linked or BetterPullReview-managed clone, never a head snapshot download


## Observed

The owner, 2026-11-05. A sparse review (gh transport) of a large repository
with a linked clone. The PR's head had been pushed minutes earlier, so the
clone did not hold it yet. Asking the agent stopped after about 19 s:

- Card: "The agent needs every file at this head, and this review holds only
  the files it shows. Downloading the rest failed."
- Show the log: "The head's files were not downloaded for the agent: it could
  not be read (transport failure 1, step 9)."
- Diagnostics: `agent.workspace.failed action=workspace_materialize
  failure=download_failed duration_ms≈19000`.

Earlier the same day, agent questions on the same repository succeeded: the
clone held those heads, so the clone path (BPR-1772) lent the workspace.

## Cause

1. With a linked clone that lacks the head, `materialize_agent_workspace`
   (`src/application/src/epoch_application_host.cpp`, the
   `sparse && aRequest.HistoryClone` block, ~line 7375) drops the clone and
   calls `acquire_workspace_snapshot`. That runs a depth-1 `gh repo clone
   --revision=<head>` of the whole head
   (`gh_cli_git_smart_transport.cpp`, `acquire_snapshot`, ~line 1061).
2. That clone runs under the acquisition runner's file limits: 32 MiB per
   file (`pack_limit`, enforced with `RLIMIT_FSIZE` in
   `github_cli_process_runner.cpp` ~line 660) and 96 MiB per directory.
   `transfer.unpackLimit=0` makes Git write the snapshot as one pack, so any
   real head of a large repository exceeds 32 MiB in seconds.
3. Git dies or exits non-zero and the adapter returns `failure::unavailable`
   (step `clone`). `snapshot_failure` maps that to `download_failed`, "it could
   not be read", instead of `too_large`. So the BPR-1772 wording ("your clone
   doesn't have it yet …") is never shown in this case. BPR-1772 saw the
   same download fail as `response_limit` (11); this time it is `unavailable`
   (1). Both are the same size wall.

The design problem is bigger than the wording. Downloading every file of the
head for every agent question is the wrong cost model: gigabytes per question
on a large repository, when the clone already holds all but a few KB of it.

## Decision (owner, 2026-11-05)

- **Linked clone:** use it. If it lacks the head, get only what is missing.
- **No linked clone:** before an agent query, ask the reviewer to either link
  a clone or let BetterPullReview manage one. Strongly encourage one or the
  other.
- **Neither:** the agent runs on the PR only (the files the review holds), and
  says so plainly. Results will be weaker.
- **Remove the head snapshot download entirely.** It is never run
  automatically, and it is not offered behind a prompt either.

## Change

### 1. Linked clone that lacks the head: delta fetch into the private workspace

Keep the BPR-1751 form: a private disposable workspace that borrows the clone
through one `objects/info/alternates` line. Nothing is written to the
reviewer's clone (BPR-1755 Q2, RFC-BPR-0092 Q8).

- When the clone lacks the head, fetch **only the head OID** into the
  **workspace** (not the clone), over BetterPullReview's own gh credential:
  `git fetch --no-tags --no-write-fetch-head --depth=<none> <url> <head-oid>`.
  It runs through the existing gh credential-helper path the transport already
  uses for `gh repo clone`, under the same pinned private configuration and
  environment (`GIT_CONFIG_NOSYSTEM`, `GIT_CONFIG_GLOBAL=/dev/null`,
  `GIT_ALLOW_PROTOCOL=https`, hooks off, no fsmonitor).
- Git's fetch negotiation offers the alternate's refs as haves, so the server
  sends only the PR's new objects. **Verify** that negotiation really uses the
  alternate's refs, with `core.alternateRefsCommand` default or set
  explicitly. Also verify that the server accepts a want by OID (protocol v2,
  `allow-reachable-sha1-in-want` behaviour). If it does not, fall back to
  fetching the PR ref (`refs/pull/<n>/head`) into a workspace-private ref, then
  check that the ref resolves to the accepted head OID.
- Keep the existing bounds: the 32 MiB per-file cap is fine for a delta. If a
  delta really exceeds it, report `too_large` with words that say the PR's new
  objects are too large, not "could not be read".
- After the fetch, verify that the workspace holds the head commit and tree
  (`cat-file --batch-check`), as `acquire_chain` does today.
- The workspace keeps no remote and no stored refs other than what checkout
  needs, as today.
- Remove the BPR-1772 manual step ("Fetch the pull request's branch in your
  clone, then ask again") and the RFC-BPR-0092 manual `git -C CLONE fetch
  REMOTE pull/N/head` + Copy step. BPR now does it without touching the clone.

### 2. No linked clone: ask once per repository before the agent query

Before materializing the workspace for an agent query on a repository with no
linked clone, show a choice in the agent panel (one question, not a list of
warnings):

- **Link a clone…** opens the existing link-a-clone flow; then continue the
  query with step 1.
- **Let BetterPullReview keep one**: create and keep a clone owned by
  BetterPullReview under the profile's application data (alongside the
  existing per-repository cache), then continue with step 1 using it as the
  alternate. State the cost from facts BPR already has: GitHub's repository
  `size`, read during `GetRepository`, as an approximate download, and the
  location on disk. The first clone runs in the background with progress and
  can be cancelled. Later questions only fetch deltas.
- **Ask about the PR only**: proceed with only the files the review holds, and
  say so in the agent's run header ("The agent sees only this pull request's
  files"). Remember the choice per repository, and let it be changed in Agent
  settings.

Encourage the first two: make one of them the default action and give the
weaker result as the reason. Do not nag on every question once a choice is
remembered.

Open questions to settle in the RFC amendment:
- Managed clone shape: full clone (one-time cost, every file present), or
  blobless partial clone plus a checkout run by BPR (which still downloads
  every blob it checks out). Lazy blob fetches must never run inside the
  agent's sandbox or with the agent's environment.
- Whether the managed clone also replaces the sparse review cache for that
  repository (its commits and trees would make the sparse cache redundant).
  Keep this out of this card unless it falls out naturally.
- Purge and disk accounting: the managed clone belongs to the profile, appears
  in Storage, and is removed by profile purge.

### 3. Remove the head snapshot download

Delete, with their tests and fixtures:
- `acquire_workspace_snapshot` and `snapshot_failure` in
  `epoch_application_host.cpp`, and the `snapshot` branch of
  `materialize_agent_workspace` (the `HistoryClone`-absent sparse path becomes
  PR-only).
- `acquire_snapshot` in `gh_cli_git_smart_transport.cpp`, the
  `git_workspace_snapshot` port surface if nothing else uses it, and
  `gh_cli_clone_content::snapshot` with its branch in
  `compile_gh_cli_git_clone_request` (`gh_cli_transport.cpp`).
- `agent_workspace_unavailable_reason::too_large_not_in_clone` and its words
  in `agent_launch_panel.cpp` / `agent_setup_view.cpp`, if no remaining path
  produces it. Also `download_failed` / `too_large`, if only the snapshot
  produced them.
- The RFC-BPR-0092 "private copy" row text for sparse reviews ("the head's
  files only, without history …").

Amend RFC-BPR-0092 (Getting the head, The private copy, the BPR-1751
implementation note) to record this decision and supersede BPR-1738's snapshot
fetch and BPR-1772's manual-fetch wording.

## Acceptance

- AC1. Linked clone lacking the head: an agent query makes the workspace by
  fetching only the head into the private workspace. The reviewer's clone's
  refs, objects and config are byte-identical before and after (inventory
  check). The fetched pack contains only objects the clone lacks: on a fixture
  repository whose clone lacks one new commit, the pack holds that commit's
  new objects and nothing reachable from the clone.
- AC2. Linked clone holding the head: unchanged from BPR-1772, with no fetch
  and an empty copy.
- AC3. No clone and no remembered choice: the agent query shows the
  link / let-BPR-keep-one / PR-only choice and does not start until one is
  chosen. Each choice continues correctly, and the choice is remembered per
  repository and editable in Agent settings.
- AC4. BPR-managed clone: created once with the stated size and location,
  cancellable, and reused by later queries, which then fetch only deltas.
  Removed by profile purge and shown in Storage.
- AC5. PR-only: the workspace holds exactly the files the review holds, and
  the run says so.
- AC6. No code path runs a head snapshot clone (`--revision=<head> --depth=1`
  without a filter, for workspace content). Covered by an
  `architecture_boundaries` or contract test, so it cannot come back.
- AC7. A delta that exceeds a bound reports `too_large` with words naming the
  PR's new objects. No size failure reports as "could not be read".
- AC8. `python3 scripts/verify_documentation.py --root .` passes with the RFC
  amendment.

## Evidence to gather first (scratch directory, outside the repo)

1. A plain `git fetch <url> <oid>` into a repository with an alternates line
   to a clone that lacks only the newest commit. Confirm that the server
   accepts the OID want and that the received pack is tiny (`GIT_TRACE_PACKET`
   / `GIT_TRACE2_EVENT` show the haves sent).
2. The same through `gh`'s credential helper, as the transport invokes it.

## Out of scope (separate cards)

- Home's discovery refresh ends `stale` on most cycles with no failed read.
  `personal_discovery_host` `fail(queue_discovery_problem::failed)` in
  `apply_metadata` / `apply_metadata_row` rejects a retained row. Yet
  `finish_cycle` records the refresh as `succeeded`, because only `CycleStopped`
  records `failed`. The diagnostic should record the stale outcome as failed
  and name which check rejected which retained row.
- A PR open started from Home is cancelled a few seconds after a discovery
  refresh starts. It restarts only when that refresh's long retained read
  finishes, despite `home_loading_yields` / `yield_discovery`. Find who
  cancels it, and log the cancelling cause on `github.pull_request.cancelled`.

## Progress

### Evidence gathered first (2026-11-05, scratch outside the repository)

On `MikeMarcin/BPR-Test` (public), a scratch clone taken before a probe
commit was pushed to `bpr-1873/delta-probe` (PR #150, opened for this probe):

1. A plain `git fetch --no-tags --no-write-fetch-head URL HEAD_OID` into a
   bare repository whose `objects/info/alternates` names the clone's objects:
   accepted by OID over protocol v2, 16 `have` lines (the clone's ref tips),
   five new objects received.
2. `gh repo clone URL STORE --no-upstream -- --bare --revision=HEAD_OID
   --reference=CLONE …` (gh's own credential helper, as the transport runs
   it): the same 16 haves, a 3,467-byte pack of five objects, none of them in
   the clone; the clone's files byte-identical before and after.
3. A second commit that edited a file the clone held: the received pack
   carried four new objects plus one delta base the clone already had (index
   `--fix-thin`); `rev-list --objects HEAD --not --alternate-refs` and
   `pack-objects` gave exactly the four new objects. The transport therefore
   repacks that list instead of handing the received pack on.

`X-RateLimit-Remaining` was 4,994 before the run.

### Slice 1: change 1, change 3, AC6 and AC7

- The sparse workspace borrows the linked clone; when the clone lacks the
  head, `acquire_workspace_delta` fetches only what it lacks (above) into a
  private store, and the workspace indexes that pack (never shallow).
- The head snapshot download is gone: `acquire_workspace_snapshot`,
  `snapshot_failure`, `acquire_snapshot`, `git_workspace_snapshot`,
  `git_smart_workspace_snapshot_request`, `git_workspace_snapshot_pack` and
  `gh_cli_clone_content::snapshot`. `too_large_not_in_clone` became
  `clone_needed`: a sparse review with no linked clone stops with "the agent
  works in a clone of the repository" until change 2's choice lands.
- `architecture_boundaries` refuses the removed names and any depth-limited
  clone without `--filter=blob:none` (checked to fail when the filter is made
  conditional).
- RFC-BPR-0092 is amended (The private copy, the BPR-1751 note, Check it
  works) with the owner's decision and the open questions' answers: a full
  managed clone, the sparse cache left as it is, the managed clone in Storage
  and removed by purge.

Focused checks, all passing on the slice: `gh_cli_git_smart_transport`
(AC1's exact object set and clone inventory, AC7's two size walls, AC6's
compiled arguments), `git_process_contract` (the workspace from the clone
plus the delta, the clone unchanged), `application_epoch_host` (AC2, the
delta path, failure words, `clone_needed`), `qt_agent_launch_panel`,
`qt_agent_setup_view`, `core_operation_diagnosis`,
`application_agent_invocation_host`, `application_release_maintenance`,
`architecture_boundaries`, the format check and the documentation gate.

### Left for slice 2

Change 2: the no-clone choice in the agent panel (AC3), the
BetterPullReview-managed clone with its stated size, progress, cancel, reuse,
Storage and purge (AC4), and the PR-only workspace with its run header
(AC5).
