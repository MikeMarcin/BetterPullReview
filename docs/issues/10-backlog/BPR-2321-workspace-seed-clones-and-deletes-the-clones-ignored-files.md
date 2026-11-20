---
schema: "implement-rfc.card.v1"
id: "BPR-2321"
title: "An agent workspace seeded from a linked clone copies the clone's ignored files with each top-level folder and then deletes them: on a clone with 750,000 ignored files the workspace still takes 150 s"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-20, build 71 on macOS (APFS): the Agent tab's workspace took 2.5 minutes on a large repository with a linked clone, after BPR-2289, BPR-2294 and BPR-2295 had landed. The log and a timed reproduction on a scratch copy of the clone's largest folder found the cause","related_cards":["BPR-1751","BPR-2288","BPR-2289","BPR-2293","BPR-2294","BPR-2295"],"related_rfcs":["RFC-BPR-0092","RFC-BPR-0108"]}
acceptance: []
---

# BPR-2321: An agent workspace seeded from a linked clone copies the clone's ignored files with each top-level folder and then deletes them: on a clone with 750,000 ignored files the workspace still takes 150 s

[[BPR-2294]] made a workspace on a large head "ready in seconds" by cloning
the linked clone's top-level folders copy-on-write and seeding the index from
the clone's. A top-level folder's clone takes **everything** in it, ignored
files included, and `clean -ffdx` then deletes those files one by one. On a
clone whose ignored files outnumber its tracked ones ten to one, that copy
and delete is the whole cost.

## Observed

The owner, 2026-11-20, build 71 on macOS (APFS, one volume). It was a large
C++ repository with a linked clone at the default branch. The Agent tab
showed "making its workspace" for about two and a half minutes. The log:

```text
10:07:26.271 agent.workspace.started   action=workspace_materialize
10:09:56.302 agent.workspace.succeeded action=workspace_materialize duration_ms=150029
             source=borrowed sparse=1 clone=1 clone_head=1 git=none git_error=none
10:10:00.896 agent.workspace.succeeded action=workspace_cleanup duration_ms=4593
```

`source=borrowed clone=1 clone_head=1`: the [[BPR-2289]] borrowing and the
[[BPR-2294]] seed path were taken. The Standard log names no step inside
the 150 s.

### The linked clone

Counted with `git ls-files --others --ignored --exclude-standard --directory`
(read-only) and `find`:

| | Files |
|---|---|
| Tracked at the head | 79,755 |
| Ignored, under the one top-level folder that holds most of the source | about 750,000 |
| largest: a vendored dependencies folder's two package trees | 181,604 and 100,631 |
| Total in that top-level folder | 800,593 |

Dependency checkouts, build output and tool caches of this size are normal
in a large C++ checkout. They sit inside tracked folders, not beside them.

### Timed on a scratch copy

The clone's folder was cloned to a scratch folder on the same volume with
the same `clonefile` call BetterPullReview makes. Nothing was written to the
clone, and the scratch folder was removed afterwards.

| Step | Time |
|---|---|
| `clonefile` of the one folder (800,593 files) | 36.8 s |
| Deleting its ignored files (what `clean -ffdx` does) | 106 s |
| **Sum** | **≈143 s of the 150 s** |
| Removing the remaining 74,841 tracked files (scratch clean-up only, not a workspace step) | 11 s |
| `find` listing all 800,593 files | 2 s |

## Cause

`seed_from_linked_clone` (`src/adapters/git/src/installed_git_process.cpp`,
around lines 4935–5030):

1. Lists the head's top-level trees, and for each one that is a real folder
   in the clone's working tree, calls `::clonefile` on the **whole folder**
   (around lines 4970–4991). `clonefile` copies the folder recursively. It
   cannot leave out the files Git ignores.
2. After seeding and refreshing, runs `clean -ffdxq` (around line 5023) so
   that "nothing else is in it". On this clone that deletes about 750,000
   files the workspace never needed.

The [[BPR-2294]] measurements (844–958 ms) used a 20,003-file fixture whose
clone held one untracked note and one small ignored build folder. They could
not show a cost that grows with the clone's ignored content.

## Change

### 1. Clone only folders with nothing extra in them

- Walk the clone's working tree once, folder by folder, without following
  links, and compare each folder's entries with the head's tree at that
  path (`ls-tree -r -t -z --format=…` once, in the workspace, never in the
  clone).
- A folder whose working-tree entries are all in the head's tree, all the
  way down, is cloned whole with one `clonefile`. A folder that holds
  anything extra (ignored, untracked, nested repository) is made empty in
  the workspace, and the walk goes one level into it. A file the head holds
  in it is cloned on its own (one `clonefile`), or left for `checkout-index`
  if it is missing or a link.
- The walk lists folders and does no reads or hashes. On the clone above, a
  full listing took 2 s.
- Keep everything [[BPR-2294]] already guards: no submodules, same volume
  only, the clone's index copied, never shared, its skip-worktree and
  assume-unchanged refusal, `core.checkStat=minimal` and `core.trustctime=false`,
  and the final `diff-files --quiet` plus empty `ls-files --others`.
- `clean -ffdx` stays as the last guard. Unless the clone changed between
  walk and clone, it now finds nothing.

### 2. Bound the walk

- If the walk finds the clone's extra entries so spread out that the clone
  calls would exceed a set count (say, more than about 50,000 separate
  `clonefile` calls), stop seeding and check out every file, as without a
  clone. Measured on this repository, outside BetterPullReview: `read-tree`
  0.15 s and `checkout-index -a` 15.2 s for the head's 79,739 files. So on
  this clone, today's seed path is ten times **slower** than the fallback it
  was meant to beat.

### 3. Log the steps

- `workspace_materialize` names how long the seed's walk, clones, refresh,
  checkout of differing files and clean took. It also names how many
  folders and files were cloned, how many were written, and how many files
  `clean` removed. A clean that removes thousands of files is a regression
  worth seeing in a Standard log.

### 4. Tests

- Git process contract: a retained workspace whose linked clone holds an
  ignored folder of 100,000 small files inside a tracked top-level folder
  is ready within the [[BPR-2294]] bound for its tracked file count, clones
  none of the ignored files (counted in the trace), and `clean` removes
  none.
- The same course with untracked files scattered across many folders
  either seeds within the bound or, past the Change 2 limit, falls back to
  a full checkout. The log names which.
- [[BPR-2294]]'s existing courses (clone dirty file, untracked notes, ignored
  build folder, skip-worktree refusal, gc after seeding) pass unchanged.

## Acceptance

- AC1. On a linked clone whose tracked folders hold ten times their tracked
  files in ignored content, an agent workspace is ready in about the time
  [[BPR-2294]] measured for its tracked file count. Its time does not grow
  with the ignored content.
- AC2. Seeding never copies a file the head does not track, and the final
  `clean` removes nothing in the normal case.
- AC3. The workspace log line names the seed's steps and counts, including
  the files `clean` removed.
- AC4. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- The two retained workspaces left over from before [[BPR-2289]] (8.6 and
  8.7 GB, each with a private pack). [[BPR-2293]]'s expiry and Settings'
  Remove clear them.
- Windows and Linux, which have no seed path and check every file out.
- Also seen in the same log, while the workspace was being made: the
  agent's status check took 22.3 s, one of its two runs ending in a process
  timeout, through a sandbox launcher. It was not on the workspace's path,
  but it may deserve its own look.
