---
schema: "implement-rfc.card.v1"
id: "BPR-2321"
title: "An agent workspace seeded from a linked clone copies the clone's ignored files with each top-level folder and then deletes them: on a clone with 750,000 ignored files the workspace still takes 150 s"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-20, build 71 on macOS (APFS): the Agent tab's workspace took 2.5 minutes on a large repository with a linked clone, after BPR-2289, BPR-2294 and BPR-2295 had landed. The log and a timed reproduction on a scratch copy of the clone's largest folder found the cause","related_cards":["BPR-1751","BPR-2288","BPR-2289","BPR-2293","BPR-2294","BPR-2295"],"related_rfcs":["RFC-BPR-0092","RFC-BPR-0108"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "eca6d5621", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^git_process_contract$'", "detail": "The BPR-2294 fixture (20,003 tracked files) with 100,000 ignored files (empty) in module3/build/dependencies: seeded in 831-1162 ms over three runs, against 935-964 ms for the same clone without them and 1.7-2.7 s for a full checkout. LINKED_CLONE_IGNORED:files=20003:ignored=100000:seeded_ms=867:baseline_ms=950:walk_ms=77:clone_ms=189:refresh_ms=260:checkout_ms=16:clean_ms=150:folders=98:files=402:written=3:cleaned=0 (Debug, macOS arm64, APFS). The owner's repository was not used."}}, {"criterion": "AC2", "outcome": "pass", "revision": "eca6d5621", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^git_process_contract$'", "detail": "Seeded: 98 whole folders and 402 single files cloned (module2's and module3's files and the two top-level files); the clone's notes, ignored build folder and 100,000 ignored files are absent from the workspace, and clean removed 0 entries in every seeded course. Untracked files in 10 folders: seeded, 2,402 files cloned one by one, clean removed 0. In 60 folders: 12,440 clone calls exceed min(50,000, 20,003/2), the seed is skipped as too_spread and all 20,003 files are checked out (1.5-1.8 s). BPR-2294's dirty-file, untracked-notes, ignored-build, skip-worktree refusal and gc-resume checks pass unchanged."}}, {"criterion": "AC3", "outcome": "pass", "revision": "eca6d5621", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^core_operation_diagnosis$'", "detail": "workspace_materialize gains `seed=seeded|too_spread|refused|failed seed_walk_ms seed_clone_ms seed_refresh_ms seed_checkout_ms seed_clean_ms seed_folders seed_files seed_written seed_cleaned` when a seed was tried, round-trips through parse_operation_diagnosis, and is absent otherwise. seed_cleaned counts the entries clean reported (a whole untracked folder is one)."}}, {"criterion": "AC4", "outcome": "pass", "revision": "eca6d5621", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(architecture_boundaries|core_operation_diagnosis|git_process_contract)$' && cmake --build --preset dev --target coding-format-check && python3 scripts/verify_documentation.py --root .", "detail": "All three tests pass (git_process_contract 60-73 s, limit 180 s); clang-format 22 clean on the changed files (the only finding is the known tests/end_to_end/follow_up_public_path.cpp); clang-tidy passes on the changed sources; DOCUMENTATION_OK."}}]
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

## Fix

`seed_from_linked_clone` (`src/adapters/git/src/installed_git_process.cpp`,
macOS) reads the head's tree once in the workspace (`ls-tree -r -t -z`),
then lists the clone's folders along the head's own, without following
links. A folder whose entries are all the head's, of the same kind, all the
way down, is cloned whole; any other is made empty and each of the head's
regular files in it is cloned on its own. Links and what the clone lacks
are checked out. No folder the head does not track is listed or copied.

The bound (Change 2) is `min(50,000, half the head's files)` clone calls,
counted before anything is cloned. The owner's measurement on the real
repository (`read-tree` 0.15 s, `checkout-index -a` 15.2 s for 79,739 files,
about 190 µs a file) and [[BPR-2294]]'s (one `clonefile` per file 3.0 s for
22,000, about 136 µs; a fixture checkout 80-110 µs a file) put the break-even
between one clone per file and a full checkout above half the files on both,
so a seed under the bound is never slower than the fallback it replaces.

Every [[BPR-2294]] guard is kept. `clean -ffdx` remains the last guard and
its removed entries are counted. The `workspace_materialize` line names the
outcome, each step's time and the counts (core `workspace_seed_trace`).

## Measured

Debug, macOS arm64, APFS, `git_process_contract`, three runs:

| Course | Time |
|---|---|
| Seeded, clone with untracked notes and one ignored folder (BPR-2294) | 935-964 ms |
| The same plus 100,000 ignored files | 831-1162 ms (walk 77-398, clone 188-195, refresh 229-263, checkout 16-18, clean 139-150) |
| Untracked files in 10 of 100 folders (seeded) | 981-1127 ms |
| Untracked files in 60 of 100 folders (too_spread, full checkout) | 1514-1801 ms |
| Full checkout, no clone index | 1741-2693 ms |

## Not verified

- The owner's repository (79,739 tracked, about 750,000 ignored files) was
  not used; its workspace time is the owner's to see in the next build's
  `workspace_materialize` line.
- The fixture's ignored files are empty, so their clone or delete cost in
  the old path would have been mostly metadata; the new path never lists them.
- Windows and Linux are unchanged (no seed path).

