---
schema: "implement-rfc.card.v1"
id: "BPR-2345"
title: "An agent workspace still takes about a minute on a large repository: the seed reads the head's tree with quoted paths, so a non-ASCII file name sends it to a full checkout, and Agent setup builds a whole workspace only to delete it before the turn builds it again"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2345--BPR-2345--fd28b5ae63f3.json","acceptance":["AC1","AC2","AC3","AC4","AC5"],"owner_step":"pending","found_by":"owner, 2026-11-21, build 72 on macOS (APFS): a new Agent tab workspace on a large repository's pull request still felt like about 45 s after BPR-2321. The BPR-2321 log fields showed seed=failed on both of two workspace builds; a scratch reproduction of the seed, read-only on the linked clone, found the cause and confirmed both fixes","related_cards":["BPR-2256","BPR-2261","BPR-2289","BPR-2293","BPR-2294","BPR-2295","BPR-2321"],"related_rfcs":["RFC-BPR-0092","RFC-BPR-0108"]}
acceptance: []
---

# BPR-2345: An agent workspace still takes about a minute on a large repository: the seed reads the head's tree with quoted paths, so a non-ASCII file name sends it to a full checkout, and Agent setup builds a whole workspace only to delete it before the turn builds it again

[[BPR-2321]] made the [[BPR-2294]] seed clone only the linked clone's folders
the head tracks whole, and logged each step. On the owner's repository, the
first run with it found two more problems:

- **A.** The seed fails its final check whenever the head holds a file whose
  name is not ASCII and that the seed then checks out with a different size
  from the clone's. It then checks every file out instead (14–17 s), after
  doing all the seed's work.
- **B.** Choosing an agent on the Agent tab builds a complete workspace to
  check that one can be made, deletes it, and the first turn then builds the
  same workspace again.

Together they cost about 61 s. With both fixed the seed measured 7.1–7.2 s.

## Observed

The owner, 2026-11-21, build 72 on macOS (APFS, one volume). It was a large
C++ repository (about 80,000 files at the head) with a linked clone at the
default branch, and a pull request with no retained workspace. The log:

```text
.210.9 agent.setup_check.started  action=agent_status_check
.210.9 agent.workspace.started    action=workspace_materialize
.239.9 agent.workspace.succeeded  action=workspace_materialize duration_ms=28931
       source=borrowed sparse=1 clone=1 clone_head=1 seed=failed
       seed_walk_ms=2817 seed_clone_ms=1568 seed_refresh_ms=2531 seed_checkout_ms=17499
       seed_clean_ms=676 seed_folders=904 seed_files=2335 seed_written=2142 seed_cleaned=0
.246.6 agent.workspace.succeeded  action=workspace_cleanup duration_ms=6748
.246.7 agent.workspace.started    action=workspace_materialize
.271.9 agent.workspace.succeeded  action=workspace_materialize duration_ms=25228
       source=borrowed sparse=0 clone=1 clone_head=1 seed=failed
       seed_walk_ms=1773 seed_clone_ms=1741 seed_refresh_ms=2364 seed_checkout_ms=14273 ...
.271.9 agent.run.started
```

After a fallback, `seed_checkout_ms` and `seed_clean_ms` are the full
checkout's ([[BPR-2321]]), so the log does not say which seed step failed.

### Reproduced on a scratch folder

The seed was re-run step by step in a scratch workspace on the same volume:
the same `ls-tree`, walk, plan, `clonefile` calls (`CLONE_NOFOLLOW |
CLONE_NOOWNERCOPY`), copied index, `config`, `read-tree --reset`, refresh,
`checkout-index`, `clean -ffdx` and final check, with Git's global and
system config off and the LFS filters cleared as `workspace_git_arguments`
clears them. Git ran only in the scratch folder, reading the clone's objects
through alternates. Nothing was written to the clone.

| | Folders cloned | Files cloned | Written | Final check | Seed time |
|---|---|---|---|---|---|
| As shipped | 904 | 2,335 | 2,142 | **4 files modified**: fails | 7.6 s, then the full checkout |
| `ls-tree` paths unquoted | 742 | 2,066 | 2,130 | clean | 7.2 s |
| `checkout-index -u` | 904 | 2,335 | 2,142 | clean | 7.1 s |

The counts in the first row are the log's. The four modified files were
LFS-tracked test resources whose names hold `ø` (UTF-8 `c3 b8`).

## Cause

### A. The head's tree is read with quoted paths

1. `seed_from_linked_clone` lists the head with
   `ls-tree -r -t -z --format=%(objectmode) %(path)`
   (`src/adapters/git/src/installed_git_process.cpp`, line 4987). With
   `--format`, `%(path)` is quoted as `core.quotePath` says **even with
   `-z`** (seen on Git 2.54.0). A non-ASCII path comes out as
   `"dir/\303\270….xml"`, quotes included.
2. The quoted entries are filed under a parent whose name starts with `"`,
   so the real folder's head entry lacks them. The walk then finds those
   files in the clone's folder as "not the head's": the folder is impure, and
   the files are not cloned. Nine files were affected.
3. The refresh finds them missing, and the seed checks them out
   (`checkout-index --force -z --stdin`, line 5178). With the LFS filters
   cleared, an LFS file is written as its pointer (129 bytes).
4. `checkout-index` without `-u` leaves the index entry's stat data as the
   clone's index had it: `read-tree --reset` keeps it for an unchanged blob.
   The clone holds the LFS content (1,870 bytes) with matching stat data.
5. On the final refresh, Git sees a size different from a non-zero recorded
   size and reports the file modified without hashing it. `diff-files
   --quiet` (line 5196) fails, and the seed's whole result is discarded.

The five non-LFS files among the nine pass, because their size matches and
the refresh hashes them. Any head with a non-ASCII path whose checked-out
size differs from the clone's recorded size fails the same way: LFS files,
and files the clone holds with other line endings.

The quoting is also wrong when nothing fails. The affected folders are cloned
file by file instead of whole (904 folder and 2,335 file clones, against 742
and 2,066 unquoted).

### B. Agent setup makes a workspace and deletes it

- When an agent is chosen, Agent setup's workspace check calls
  `probe_workspace` (`src/app/src/github_application/window_agent_setup.cpp`,
  around line 1620).
- With no retained workspace for the pull request, the probe materializes
  one under a reserved session identity and removes it at once
  (`src/application/src/agent_invocation_host.cpp`, around lines 2138–2142,
  [[BPR-2261]]).
- The first turn then finds no retained workspace and materializes it again.
  On this repository that is 28.9 s to build, 6.7 s to remove, then 25.2 s to
  build it again. The probe built with `sparse=1` and the turn with
  `sparse=0`, so the two requests were not identical.

## Change

### 1. Read the head's paths unquoted

- Read the tree without quoting: `-c core.quotePath=false` on that `ls-tree`,
  or Git's default `-z` output (`<mode> SP <type> SP <oid> TAB <path>`),
  which `-z` never quotes. Prefer the default output if `--format` quoting
  differs between Git versions.
- Refuse (as `refused`) any listed path that still begins with `"`, so a
  quoting change in a later Git falls back openly instead of failing late.
- Check every other `--format=…%(path)` or `%(path)`-style read in the
  adapters for the same quoting.

### 2. Record what the seed writes

- The seed's `checkout-index` of the differing files passes `-u`, so the
  index takes the written files' stat data, whatever the clone recorded.
- When the final check still names modified files, check those files out
  once more with `-u` and check again before falling back. Name the count in
  the log.

### 3. Name the step that failed

- `workspace_materialize` with `seed=failed` names the failed step
  (`seed_step=list|walk|clone|read_tree|refresh|checkout|clean|verify`) and,
  for `verify`, how many files were modified and how many untracked. Keep the
  seed's own checkout and clean times apart from the fallback's.

### 4. Keep the probe's workspace

- When Agent setup's probe has to make a workspace, it keeps it as the pull
  request's retained workspace under the [[BPR-2261]] key, with the
  [[BPR-2293]] expiry, instead of removing it. The first turn reattaches it
  through the existing inspection, so the workspace is built once.
- The probe asks for the same workspace a turn would ask for (the same
  sparse or complete choice, profile and binding), or the turn's check
  refuses it as it would refuse any mismatched workspace.
- If keeping the probe's workspace is refused, for example because the probe
  must not hold disk, the probe proves less: it checks the inputs (clone,
  head, volume, history) without materializing, and the turn builds the
  workspace. Decide which, and record why.

### 5. Tests

- Git process contract: a head with a non-ASCII file name (`ø`, and a
  combining-mark name) in a tracked folder, held by a linked clone with
  different content of a different size for that file (an LFS-style
  pointer against its content), is seeded (`seed=seeded`). The folder is
  cloned whole, and nothing falls back.
- The same course with `checkout-index` forced to write that file: the final
  check passes.
- A listing with a path still quoted is `refused`, not `failed`.
- Application: Agent setup's probe on a pull request with no retained
  workspace leaves one retained, and the first turn reattaches it with no
  second `workspace_materialize`.
- [[BPR-2294]] and [[BPR-2321]]'s existing courses pass unchanged.

## Acceptance

- AC1. On a linked clone whose head holds non-ASCII file names, including LFS
  files, the workspace is seeded, and `seed=seeded` is logged.
- AC2. A seed that fails names the step it failed at in the Standard log.
- AC3. Choosing an agent and starting the first turn on a pull request with
  no retained workspace builds one workspace, not two, and removes none.
- AC4. On the owner's repository, the time from choosing the agent to the
  turn starting is about the seed's time (under 10 s), not about a minute.
- AC5. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Owner step

On the corporate Mac, with the same repository and its linked clone: open a
pull request with no retained workspace, choose the agent, and start a
turn. The log shows one `workspace_materialize` with `seed=seeded`.

## Out of scope

- Windows and Linux, which have no seed path. Change 4 applies to them too.
- `core.fsync=all` with `core.fsyncMethod=fsync` on workspace Git, which
  makes a full checkout fsync every file. That is worth measuring on the
  fallback, but it is not the cause here.
- In the same session, a comparison's historical acquisition timed out after
  61 s (`mode=complete_closure source=host linked_clone=0`) although the
  linked clone held both commits. It is carded separately once a Verbose log
  confirms it.

## Resolution

### 1. The head's paths, unquoted

`seed_from_linked_clone` (`src/adapters/git/src/installed_git_process.cpp`)
reads the head with Git's own `ls-tree -r -t -z` lines (`<mode> SP <type>
SP <oid> TAB <path>`), which `-z` never quotes, instead of
`--format=%(objectmode) %(path)`. Confirmed in a scratch repository on Git
2.50.1 as well as the owner's 2.54.0: `--format`'s `%(path)` is quoted with
`-z`, the default line is not. A listed path that still begins with `"`
answers `seed=refused` and every file is checked out.

The other path-bearing reads in the adapters were checked: every other
`ls-tree`, `ls-files`, `diff-files`, `diff-tree --name-status`, `diff
--name-status` and `status --porcelain=v2` read uses `-z` without a
`%(path)` format, and the remaining `--format`s name objects, refs, times
or subjects (`%(objectname)`, `%(refname)`, `%ct`, `%s`). None needed a
change.

### 2. What the seed writes is recorded

The seed's `checkout-index` passes `-u`, so the index takes the written
files' stat data. When the final check still finds files modified (and none
untracked), they are checked out once more with `-u` and checked again
before the seed gives up; the count is `seed_rewritten`.

### 3. The failed step is named

`core::workspace_seed_trace` gains `Step` (`seed_step=list|walk|clone|
read_tree|refresh|checkout|clean|verify`), `Modified` and `Untracked`
(written on every `seed=failed` line, non-zero only for `verify`),
`Rewritten`, and `fallback_checkout_ms`, `fallback_clean_ms` and
`fallback_cleaned`, written whenever the seed did not seed.
`seed_checkout_ms`, `seed_clean_ms` and `seed_cleaned` are now always the
seed's own. The line round-trips through `parse_operation_diagnosis`, and a
trace that breaks these rules (a failed seed with no step, a step on a
seeded one, counts on a step other than `verify`, fallback times on a
seeded one) is no line. The seed resets its trace at each attempt.

### 4. The probe's workspace is kept

Decision: **keep**. The probe can ask for exactly the workspace a turn asks
for. Both go through the same `materialize_agent_workspace` with the same
application session, authority, binding, parent and clone; the probe now
also passes `HistoryCloneManaged` (the window gives it
`linked_history_clone_managed(false)`, as a chat turn's request does) and
`RetainForSession` (whenever the host has a session store, as a native
turn's does). What differs is the session identity and profile. A reattach
uses the identity the kept record carries, as it already does for a
workspace another conversation made, so the turn's check accepts it. The
`sparse=1` against `sparse=0` in the owner's log is not a request field: it
is decided by how the review's cache was loaded
(`epoch_host_materialization.cpp`), and the reattach inspection checks the
workspace whatever made it.

Probing without materializing was rejected. It proves less (no seed, no
checkout, no borrowed-history check), and the turn would still pay the
whole build after the agent was chosen. Holding the disk is what a turn
does anyway, and the kept workspace sits under the same [[BPR-2293]]
expiry and Settings' Remove as any other.

How: `native_agent_session_store_port::keep_workspace(key, binding,
workspace)`. The SQLite store keeps it in a new
`native_agent_kept_workspaces` table, keyed by the [[BPR-2261]] pull
request key, with the columns a conversation keeps its workspace in (and
its sparse evidence). `shared_workspace` answers it to a conversation
holding none. A turn that stores its conversation with the key takes it
over (the kept row is deleted in the same transaction). `retained_workspaces`
lists it once, for expiry, from when it was kept (`native_agent_workspace_use`).
It counts as ended (removed first by the policy sweep) once a conversation
holds another workspace under its key, or once its pull request has a
selected conversation and none is at its base and head; with no
conversation yet it waits for its days. `forget_workspace` clears it. The host keeps it only when the store
accepts it. When a conversation already holds a workspace under the key,
another is kept, or the write fails, the probe removes its own as before.
The probe still releases the pull request's lease while it builds, so a
turn sent meanwhile is not refused; it builds its own, and the store then
refuses to keep the probe's.

A first turn that is not a native session (writing-only, an existing
checkout) does not reattach it; the kept workspace then waits for a native
turn or its expiry.

### 5. Tests

- `git_process_contract`, `run_linked_clone_unicode_contract` (macOS): a
  clone with `assets/fø.bin` (LFS-style: the clone's clean filter stores a
  129-byte pointer for 1,870 bytes of content, so the clone's index records
  1,870), `assets/x\u0301.txt` and ten other folders. The head is seeded,
  with all 11 folders cloned whole, only `.gitattributes` cloned on its own,
  nothing rewritten and no fallback. With the LFS file missing from the clone
  the folder is still cloned whole and the file checked out as its pointer,
  and the seed passes its final check. A head with a top-level `"quoted.txt`
  is `refused` (step none) and checked out whole. The clone's index and
  HEAD are unchanged.
- Mutation checks, the same course: with the old `--format` listing it fails
  (`seed=refused` through the new quote refusal, nothing cloned); with
  `checkout-index` without `-u` it fails at the second case with
  `seed=failed seed_step=verify seed_rewritten=1 seed_modified=1`, the
  owner's failure, now named.
- `core_operation_diagnosis`: the new keys, their rules and round trip.
- `application_agent_invocation_host`,
  `test_workspace_probe_keeps_the_workspace_for_the_first_turn`: the probe
  makes one retained workspace and keeps it under the pull request key; the
  first Agent tab turn reattaches it (one materialization, one inspection,
  no cleanup, the launch's working folder is the probe's); a second check
  makes nothing. The [[BPR-1751]] probe test now asks the store to refuse,
  and still sees the workspace made and removed, carrying the managed-clone
  flag and a retained request.
- `sqlite_agent_setup_storage`: keep, refuse a second or a held key or
  another head, read after a restart with the reserved identity and sparse
  evidence, conflict at another head, take-over by the first turn, listed
  once throughout, forgotten; one kept for a head the pull request's
  selected conversation has moved past is ended.

## Evidence

Tested at `869339e79` (branch `bpr-2345-workspace-seed` on main
`4dd1f89b7`), dev preset (Debug), macOS arm64, APFS. Landed rebased over
`88c765f7b` ([[BPR-2344]]: the Qt report summary and one paragraph of
`ai-agent-boundary.md`, no file this card touches but that document); the
documentation gate and the composition digests were run again after the
rebase. Fixtures were made in scratch
folders by the tests and removed by them; the owner's repositories were not
used.

| Criterion | Check | Result |
|---|---|---|
| AC1 | `git_process_contract` (62.8 s; limit 180 s): `LINKED_CLONE_UNICODE:outcome=seeded:step=none:folders=11:files=1:written=1:rewritten=0`, then with the LFS file missing `folders=11:files=1:written=2:rewritten=0`, then `outcome=refused` for the quoted head. BPR-2294/2321 courses unchanged: `LINKED_CLONE_CHECKOUT seeded_ms=909 full_ms=1877`, `LINKED_CLONE_IGNORED seeded_ms=857 cleaned=0`, `SCATTERED:folders=10 seeded`, `SCATTERED:folders=60 too_spread written=20003` | Passed; both mutation checks fail as described |
| AC2 | `core_operation_diagnosis`; the `-u` mutation run logged `seed_step=verify seed_modified=1` | Passed |
| AC3 | `application_agent_invocation_host` (`test_workspace_probe_keeps_the_workspace_for_the_first_turn`), `sqlite_agent_setup_storage`, `retained_workspace_expiry` | Passed |
| AC4 | The owner's repository, from choosing the agent to the turn starting | Pending: owner step |
| AC5 | `architecture_boundaries` (23.3 s), the tests above, `python3 scripts/verify_documentation.py --root .`, clang-format 22 on the 12 changed C++ files, clang-tidy on the changed translation units; the composition contract's pinned digests through its inventory mode (`ASYNC_MUTATION_HOST_INVENTORY_OK`) after the [[BPR-2345]] manifest reseal | Passed |

`application_review_round_trip_composition_contract` itself was not run:
it needs the full application, which this lane did not build. Its digest
checks were run through the contract's own inventory mode. The first full
dev run on main should include it.

## Owner step (pending)

As above, on the corporate Mac with the same repository and linked clone:
open a pull request with no retained workspace, choose the agent, and start
a turn. Expected in the Standard log: one `workspace_materialize` with
`seed=seeded` (no `workspace_cleanup` after it), then the turn's workspace
check and `agent.run.started`. The time from choosing the agent to the
turn starting should be about the seed's (under 10 s). If the seed still
fails, the line now names `seed_step`, and for `verify` how many files
were modified and untracked.
