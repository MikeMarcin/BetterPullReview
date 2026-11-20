---
schema: "implement-rfc.card.v1"
id: "BPR-2294"
title: "An agent workspace on a large head is ready in seconds: checked out from the linked clone's index as a cache, or sparsely, rather than writing every file of the head"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2293--BPR-2294--e7efc87e257e.json","parent":"BPR-2288","origin":"split from BPR-2291 (2026-11-19): BPR-2288 Change 5 option 3 and AC5's 'ready in seconds'","related_cards":["BPR-2288","BPR-2289","BPR-2291","BPR-1751","BPR-1738"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "21cae6864", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^git_process_contract$'", "detail": "run_linked_clone_checkout_contract: a retained workspace on a 20,003-file head one commit from its linked clone (clone files dated a year back) writes 5 files (the changed, the added, the clone's dirty one, two top-level files) and is ready in 844-958 ms over four runs; the same head with no clone index to seed from writes 20,003 and takes 1.6-2.2 s. LINKED_CLONE_CHECKOUT:files=20003:seeded_ms=844:seeded_written=5:full_ms=1877:full_written=20003 (Debug, macOS arm64, APFS, machine busy with other builds)."}}, {"criterion": "AC2", "outcome": "pass", "revision": "21cae6864", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(git_process_contract|retained_workspace_expiry|agent_workspace_ownership)$'", "detail": "Same course: the workspace is the head and clean (status --porcelain empty), every file is there, the clone's unsaved edit, untracked notes and ignored build folder are not; the clone's index bytes and HEAD are unchanged; an edit in the workspace does not reach the clone; after the clone's gc --prune=now the workspace resumes and keeps the edit. A clone index that hides a file (skip-worktree) is refused as a cache and every file is checked out. BPR-2289's borrowed-history and incomplete-clone courses pass unchanged."}}, {"criterion": "AC3", "outcome": "pass", "revision": "21cae6864", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^architecture_boundaries$' && python3 scripts/verify_documentation.py --root .", "detail": "architecture_boundaries passed; DOCUMENTATION_OK; clang-format 22 and clang-tidy clean on the changed lines."}}]
---

# BPR-2294: An agent workspace on a large head is ready in seconds

Part of [[BPR-2288]] (Change 5 option 3, AC5's "ready in seconds"). Split
from [[BPR-2291]].

## Why

[[BPR-2289]] made a retained workspace borrow the linked clone's history, so
its own pack is only what the clone lacks. Its checkout still runs
`read-tree --reset -u` over the whole head
(`src/adapters/git/src/installed_git_process.cpp`, `materialize_agent_workspace`):
on the owner's repository that is 80,733 files and 1.6 GB, written for every
workspace, which is most of the minutes the Agent tab spent "making its
workspace at the head".

## Change (choose by measurement)

1. **Index as a cache.** Seed the workspace's index from the linked clone's
   (read only; copy, never share), so `read-tree -m -u` from the clone's
   `HEAD` tree to the head writes only the files that differ, and copy (or,
   on APFS, clone with `clonefile`) the unchanged files from the clone's
   working tree where its index says they are clean. Never write to the clone.
2. **Sparse.** Or check out the pull request's changed files and their
   directories (`sparse-checkout` cone), widened on demand when the agent
   reads a path outside them; the brief says the tree is sparse.

Measure both on a fixture of a few tens of thousands of files and pick the
one the agent's tools tolerate (a sparse tree hides files from `grep`).

## Acceptance

- AC1. On a fixture head of at least 20,000 files with a linked clone near it,
  a retained workspace is ready in seconds, and writes no more than the files
  that differ from the clone (or the sparse set), measured.
- AC2. The agent's file reads and edits work as before; a reattach after the
  clone's gc still works ([[BPR-2289]]).
- AC3. `architecture_boundaries` and `python3 scripts/verify_documentation.py --root .` pass.

## Measured (choose by measurement)

On a 22,000-file fixture (174 MB): `checkout-index --all` 1.7 s; one
`clonefile` per file (`cp -c`) 3.0 s, slower; one `clonefile` per top-level
folder 0.23 s, then seeding the index and refreshing 0.2 s. Sparse was not
chosen: it hides the rest of the head from the agent's search, and widening it
on demand needs the agent to know and ask.

## Fix

`seed_from_linked_clone` (installed Git, macOS): each top-level folder of the
head that is a real folder in the reviewer-linked clone's working tree is
cloned copy-on-write with one `clonefile` call; the clone's index is copied
(never shared); `read-tree --reset` keeps its stat data where its entry is the
head's; an entry flagged skip-worktree or assume-unchanged refuses the seed;
with `core.checkStat=minimal` and `core.trustctime=false` in the workspace, a
refresh finds the cloned files clean, `diff-files` names the rest, they are
checked out, `clean -ffdx` removes the clone's untracked and ignored files, a
second refresh takes the new files' stat, and the result must be clean with
nothing untracked. A submodule in the head, another volume, or any failed step
drops the copied index and checks every file out as before, then cleans. No
Git runs in the clone and nothing is written to it.

## Not verified

- The owner's repository (80,733 files, 1.6 GB) was not used; only the 20,003-
  file fixture.
- Linux and Windows still write every file (no directory clone there); a
  BetterPullReview-managed clone (bare) is not a cache.
- The clone's untracked files exist in the private workspace, copy-on-write,
  between the folder clone and `clean -ffdx`, before any agent starts.
