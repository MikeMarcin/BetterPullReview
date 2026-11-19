---
schema: "implement-rfc.card.v1"
id: "BPR-2294"
title: "An agent workspace on a large head is ready in seconds: checked out from the linked clone's index as a cache, or sparsely, rather than writing every file of the head"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2293--BPR-2294--e7efc87e257e.json","parent":"BPR-2288","origin":"split from BPR-2291 (2026-11-19): BPR-2288 Change 5 option 3 and AC5's 'ready in seconds'","related_cards":["BPR-2288","BPR-2289","BPR-2291","BPR-1751","BPR-1738"]}
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
