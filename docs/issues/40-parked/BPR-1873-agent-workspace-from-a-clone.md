---
schema: "implement-rfc.card.v1"
id: "BPR-1873"
title: "An agent's workspace comes from a linked or BetterPullReview-managed clone, never a head snapshot download"
status: "Parked"
parked_on: "human-review"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05: agent question on a large repository's newly pushed head stopped with 'Downloading the rest failed' (transport failure 1, step 9)","related_cards":["BPR-1738","BPR-1751","BPR-1755","BPR-1772"],"related_rfcs":["RFC-BPR-0092"]}
acceptance: [{"criterion": "AC1 Linked clone lacking the head: only the head is fetched into the private workspace; the clone is byte-identical; the pack holds only what the clone lacks", "outcome": "pass", "revision": "2fdabb302", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(gh_cli_git_smart_transport|git_process_contract)$'", "detail": "Real Git through the fake gh against a fixture clone missing one commit: the self-contained pack holds exactly that commit, its tree and two blobs, none in the clone; the clone's files are byte-identical before and after. The workspace borrows the clone and indexes the pack, never shallow. Live on BPR-Test PR #150: 16 haves, a 3,467-byte pack of five objects none in the clone, clone unchanged."}}, {"criterion": "AC2 Linked clone holding the head: unchanged, no fetch", "outcome": "pass", "revision": "2fdabb302", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_epoch_host$'", "detail": "A clone that holds the head lends it with no delta request and no pack."}}, {"criterion": "AC3 No clone and no remembered choice: the link / let-BPR-keep-one / PR-only choice before the query, remembered per repository, editable in Agent settings", "outcome": "pending", "revision": "2fdabb302", "evidence": {"kind": "observation", "detail": "The Repository step is the once-per-repository question before any Ask, remembered and changed or forgotten in Settings > Repositories > Linked clones (qt_agent_setup_view). For a sparse review it offers linking a clone (default) and Ask about this pull request only with its cost. Let BetterPullReview keep one is not built (see AC4). The look is pending in attended session 235e4b41566ab45170a77cfaf145db2b."}}, {"criterion": "AC4 BPR-managed clone: created once with stated size and location, cancellable, reused, removed by purge and shown in Storage", "outcome": "pending", "revision": "2fdabb302", "evidence": {"kind": "observation", "detail": "Not built. A full clone needs its own acquisition path without the 32 MiB/96 MiB temporary-store limits, a long-running cancellable job with progress, a profile-owned location in Storage and purge, and GitHub's size in the setup facts."}}, {"criterion": "AC5 PR-only: the workspace holds exactly the files the review holds, and the run says so", "outcome": "pass", "revision": "2fdabb302", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(application_epoch_host|git_process_contract|qt_agent_launch_panel|core_review_skill|application_review_skill_host)$'", "detail": "With no clone the host gives the workspace exactly the head's side of base...head (deletions and submodules excepted), from held bytes, batch reads and single reads, each checked against its blob; the Git adapter sparse-checks-out exactly those paths from the cache's commit and trees: the rest of the head is absent, git status is clean, shallow at the head. The brief says private_snapshot, the review skill (bpr-version 4) tells the agent to say so, and the turn's footer says It sees / It saw only this pull request's files."}}, {"criterion": "AC6 No code path runs a head snapshot clone, guarded by architecture_boundaries or a contract test", "outcome": "pass", "revision": "2fdabb302", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(architecture_boundaries|gh_cli_git_smart_transport)$'", "detail": "architecture_boundaries refuses the removed snapshot names, any second exact-revision or depth clone compiler, and a --depth not followed unconditionally by --filter=blob:none (checked to fail when the filter was made conditional); the contract test asserts the compiled arguments."}}, {"criterion": "AC7 A delta over a bound reports too_large naming the PR's new objects; no size failure reports as could not be read", "outcome": "pass", "revision": "2fdabb302", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(gh_cli_git_smart_transport|application_epoch_host|qt_agent_launch_panel)$'", "detail": "A clone stopped at the per-file limit is response_limit whether the kernel ends gh or Git exits non-zero; the host maps it to too_large with the pull request's new objects in its words, never could not be read."}}, {"criterion": "AC8 The documentation gate passes with the RFC amendment", "outcome": "pass", "revision": "2fdabb302", "evidence": {"kind": "command", "command": "python3 scripts/verify_documentation.py --root .", "detail": "DOCUMENTATION_OK with RFC-BPR-0092's amendment."}}]
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

Slice 1 is commit `32f789f59` on `worktree-agent-a5f29357fb73aecd5`, merged
with `origin/main` at `67c681bb7`. Probe PR #150 is closed and its branch
deleted.

Until slice 2 lands, a sparse review with no linked clone cannot ask the
agent at all (it says to link a clone); before slice 1 a small repository's
head was downloaded whole. This is the owner's decision applied without its
PR-only fallback yet.

### Slice 2: the pull request only and the Repository step's choice

- `a36f51b62`: with no clone the workspace holds only the pull request's
  files (AC5), read through the review's lease and checked against their
  blobs, sparse-checked-out from the cache's commit and trees, shallow at the
  head. The brief, the review skill (`bpr-version` 4) and the turn's footer
  say so. This removes slice 1's interim refusal.
- `42a759f41`: the Repository step (already the once-per-repository question
  before an Ask, remembered, changed in Settings) offers **Ask about this
  pull request only** for a sparse review, after linking a clone, with its
  cost. `frontend-design:frontend-design` was loaded first.
- `485f17beb`, `2fdabb302`: the setup facts' `Sparse` flag, never set
  before, now follows the accepted handoff.

## Attended review

Fresh short session `235e4b41566ab45170a77cfaf145db2b`, state
`/Users/jaeger/.mask/bpr-1873-v001-20261105/attended-review`, candidate
`BPR-1873-no-clone-choice` (AC3's look, Accept/Reject), ready, Tailscale host
started for the owner. The candidate is offscreen renders of the setup view
at `2fdabb302` in light and dark: the fixture-backed frozen build reads its
reviews completely, so it cannot show a sparse review's Repository step.

### Left

The BetterPullReview-managed clone (AC4, and AC3's third option): a full
clone without the transport's temporary-store limits, a cancellable
background job with progress, its size from GitHub's `size`, a profile-owned
location shown in Storage and removed by purge, then borrowed as the linked
clone so later questions fetch only deltas. Recommended as its own card.
