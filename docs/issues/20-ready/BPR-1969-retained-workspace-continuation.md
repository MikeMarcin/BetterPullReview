---
schema: "implement-rfc.card.v1"
id: "BPR-1969"
title: "A continued tour chat turn on a large full-history repository is refused as a private-history failure because the retained workspace's object check overflows its 64 MiB output bound"
status: "Ready"
rfcs: []
depends_on: ["BPR-1951"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"mandate_revision":"retained-workspace-continuation-r1","reservation":"docs/issues/_reservations/BPR-1969--BPR-1969--d2cc48e273ce.json","kind":"defect","found_by":"owner, 2026-11-09: after a tour side-chat turn had answered, two 'Update the tour from our chat' turns each ended with 'Didn't finish: Private history couldn't record the run, so <agent> didn't start.' after 0:07 and 0:04","related_cards":["BPR-1738","BPR-1880","BPR-1951"],"related_rfcs":["RFC-BPR-0092"],"milestone_plan":{"id":"bpr-1969-retained-workspace","path":"docs/rfcs/plans/bpr-1969-retained-workspace/milestone-plan.json"}}
acceptance: []
---

# BPR-1969: A continued tour chat turn on a large full-history repository is refused as a private-history failure because the retained workspace's object check overflows its 64 MiB output bound

Allocate the real ID with `python3 scripts/allocate_issue_id.py` and replace
`BPR-1969` (file name, front matter, heading).

## Observed

The owner, 2026-11-09, a Release build of 0.4.2 (build.45). The pull request
under review is in a large repository: its full history reaches about 4.4
million objects. In a tour's side chat, a first question was answered after
3:22. The owner then pressed "Update the tour from this chat" twice. Each
turn ended at once:

```text
Update the tour from our chat.
Didn't finish: Private history couldn't record the run, so <agent> didn't start.
Stopped after 0:07.

Update the tour from our chat.
Didn't finish: Private history couldn't record the run, so <agent> didn't start.
Stopped after 0:04.
```

No agent process started. The session log and the exported diagnostics show
the first turn's `agent.workspace.started`/`succeeded` and `agent.run.started`/
`succeeded` events. The two refused turns left no event at all, and no
`workspace_cleanup` follows the first run, because the native session kept
its workspace.

## Cause

1. The tour side chat continues a native session ([[BPR-1951]]):
   `window_tours.cpp` sets `tour_launch::NativeScope`, and
   `window_agent_chat.cpp` copies it into `NativeAgentIdentity` and
   `NativeScope`. The first turn materialized a workspace with
   `RetainForSession`. The repository is not shallow, so the workspace has
   no `.git/shallow`, and `prepared_workspace::Shallow` is false.
2. On the next turn, `agent_invocation_host::run`
   (`src/application/src/agent_invocation_host.cpp`) finds the retained
   workspace and calls `Workspaces.inspect_agent_workspace` before it
   launches anything.
3. `inspect_agent_workspace` (`src/adapters/git/src/installed_git_process.cpp`)
   checks the ownership marker, journal, `HEAD` and alternates. All of
   these pass. It then runs

   ```text
   git rev-list --objects --no-object-names --missing=print <head> <required-base>
   ```

   with `StandardOutputLimit = 64U * 1024U * 1024U`. For a repository that is
   not shallow, this lists every object reachable from head and base over
   the whole history, one hash per line, only to look for `?`-prefixed
   missing entries.
4. `drain_channel` stops the process with
   `git_process_failure::standard_output_limit` once 64 MiB has been read.
   The inspection reports a failure, and `run` calls `refuseNative("The
   retained native workspace or required Git objects are unavailable. …")`.
5. `refuseNative` finishes the run as
   `agent_run_outcome::private_storage_failure`. `agent_tab_view.cpp` renders
   that outcome as "Private history couldn't record the run, so %1 didn't
   start." The `StandardError` reason that `refuseNative` passes is not
   shown and not logged.

So a check meant to confirm that the retained workspace is intact grows
with the repository's whole history, and on a large repository it can never
pass. Every continued turn of a native session is refused. Neither the
outcome nor the sentence points at the workspace check.

## Probe (2026-11-09)

Run by hand against the retained workspace, with the same arguments the
inspection uses:

```text
$ git rev-list --objects --no-object-names --missing=print <head> > out.txt
rc=0  lines=4393774  missing(?)=0  bytes=180144734  wall=18.7s
```

- No object is missing: the workspace is intact, and the check would have
  passed if it had been allowed to finish.
- The output is about 172 MiB, 2.7 times the 64 MiB bound. Reaching 64 MiB
  takes about a third of the 18.7 s run, which matches the 0:07 refusal
  (0:04 once the pack was in the file cache).
- The retained-workspace budget (`native_workspace_budget_available`, 16 GiB
  and 262,144 entries) is not the cause: 100,800 entries and 8.4 GiB, and
  the tree walks without error.
- Ownership marker, journal phase, `HEAD` = pinned head, and absent
  `objects/info/alternates` and `http-alternates` all pass.

## Change

### 1. Make the object check independent of history size

`inspect_agent_workspace` must not collect a listing whose size grows with
the repository. Pick one at pickup:

- Recommended: for a workspace that is not shallow, confirm only what a
  continuation needs. That means the head and the required base commits
  exist, and their root trees (and, if required, the trees and blobs the
  checkout uses) are present. Use bounded commands, such as
  `git cat-file --batch-check` on the named objects, or
  `git rev-list --objects --missing=print --no-walk` limited to those
  commits' trees.
- Alternative: keep the full walk, but filter the output inside the runner
  so that only `?<oid>` lines are kept. The limit then applies to missing
  objects, not to all objects. The run also needs a time bound that suits
  a history of millions of objects, and its cost (about 19 s here) is paid
  before every continued turn.

The shallow-with-sparse-missing-objects path keeps its current exact
comparison against `SparseMissingObjects`.

### 2. Classify and show the refusal truthfully

- A failed retained-workspace inspection is a workspace outcome, not
  `private_storage_failure`. Use `agent_run_outcome::workspace_unavailable`
  (the outcome [[BPR-1738]] uses for a workspace that could not be
  materialized), or give it a new named outcome. Keep
  `private_storage_failure` for genuine native-session or private-run
  storage failures.
- `refuseNative`'s reason text reaches the side chat (as
  [[BPR-1738]] does for materialization failures). Every `refuseNative` path
  writes one sanitized `agent.workspace` or `agent.run` diagnostic event with
  a stable failure name, so a refused continuation can be seen in the log
  and in exported diagnostics.
- An inspection that fails because an output or time bound was hit is
  distinguished from one that found missing objects, in both the outcome
  detail and the log.

### 3. Tests

- `installed_git_process` (or the git cache tests): a non-shallow retained
  workspace whose full `rev-list --objects` output would be larger than the
  current 64 MiB bound passes inspection. To keep the fixture small, inject a
  lower output bound or check that the arguments do not walk the history. A
  workspace that is missing the head's root tree, or the required base,
  fails inspection.
- `application_agent_invocation_host` (or the native-session tests): a
  failed retained-workspace inspection finishes as the workspace outcome,
  carries the reason, and writes a diagnostic event. Native-session storage
  failures still finish as `private_storage_failure`.
- A composed tour side-chat course with a stub agent: a second turn in the
  same native session reaches the agent and its reply lands in the side
  chat.

### 4. Documentation

- `docs/architecture/ai-agent-boundary.md` (or wherever native-session
  continuation is described): what the retained-workspace check verifies,
  and that its cost is bounded independently of history size.
- [[BPR-1951]]'s native-session description: the refusal outcome for an
  unusable retained workspace.

## Acceptance

- AC1. On a non-shallow repository whose full `rev-list --objects` output is
  larger than 64 MiB, a second tour side-chat turn in the same native
  session reaches the agent, and its answer appears in that chat.
- AC2. The retained-workspace check's output and running time do not grow
  with the number of objects in history. A test pins this by command shape
  or by an injected bound.
- AC3. A retained workspace with a missing head tree or required base is
  still refused before dispatch, and no agent process starts.
- AC4. That refusal is not reported as `private_storage_failure`. The side
  chat shows a sentence about the workspace, and the log has one sanitized
  event naming the failure.
- AC5. Native-session storage failures (`select`, `append`, `store`) are
  still reported as `private_storage_failure`.
- AC6. The documentation above is updated, and
  `python3 scripts/verify_documentation.py --root .` passes.
- AC7. The git process tests, `application_agent_invocation_host`, the
  native-session tests, `qt_tour_side_chat` and `architecture_boundaries`
  pass.

## Out of scope

- Reducing the cost of materializing a full-history workspace (101 s here).
- Changing the retained-workspace storage budget (16 GiB, 262,144 entries).
- Making native-session workspaces shallow by default.
