---
schema: "implement-rfc.card.v1"
id: "BPR-1327"
title: "Prepare changed-file comparisons before they are opened"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner chat 2026-10-08","kind":"performance","related_issues":["BPR-0967","BPR-0905","BPR-0794","BPR-0795"],"branch":"claude/bpr-1327-prefetch"}
---

# Prepare changed-file comparisons before they are opened

## Owner report (chat, 2026-10-08)

> "For even a small review it seems to not be preloading the files until I go
> to view their diff, it's quite slow to open the diff file against a live pr"

## Diagnosis

A pull request opened through the connected GitHub CLI loads its comparison
with `git_smart_acquisition_mode::sparse_commit_trees`: the clone is
`--depth=1 --filter=blob:none`, so the cache holds commits and trees and no
file contents. `object_loading_for` chooses this for every pull request whose
reported shape is consistent, small ones included (the
`sparse_capability_applies_independently_of_pull_request_size` host test
pins it). Nothing reads a blob until the reviewer selects its file.

Opening one file then does, in order, on the repository's single epoch actor
FIFO:

1. Two local `git ls-tree` lookups (path to blob at each endpoint; four for a
   rename).
2. For each side, a local `cat-file --batch-check` that misses, the durable
   sparse-residency checks, then `read_exact_blob`: three `gh` processes,
   `gh api user`, `gh api repos/{o}/{r}/git/blobs/{sha}`, `gh api user`.
3. The diff computation on the diff worker.
4. Syntax classification, which reads both blobs again through the same
   materialization port. The network bytes are never retained, so it repeats
   step 2's network reads.

Measured live against `MikeMarcin/BPR-Test` pull request 40 (3 files), with
the shipped command shape, 8 blob reads: 1,196–1,538 ms per blob (viewer
372–557 ms, blob 421–689 ms, viewer 379–557 ms). A modified file therefore
waits about 2.6 s for its two blobs before the diff can appear, and the
classifier's repeated reads add another 2.6 s on the same FIFO. A revisit
after the diff cache evicts the file pays it all again.

One GraphQL query reading the same four blobs by OID took 488–628 ms
(cost 1 point), and every returned text hashed to its blob OID.

## Acceptance

- AC1 (measure): before and after numbers for opening the first file and
  later files, on a live-shaped fixture with real fetch latency, small and
  large pull requests.
- AC2 (prepare): after a comparison is available, every changed file's
  comparison is prepared in the background in the order current file, next
  file, then the rest; a huge pull request prepares only a bounded window
  around the cursor. Opening a prepared file performs no network read and no
  diff computation.
- AC3 (bounds): preparation stays inside the existing diff cache entry and
  byte budgets, retains a bounded set of verified blob bytes, never evicts a
  comparison the reviewer opened, reads missing blobs in batches, and is
  cancelled by a pull-request, comparison or account change and by close.
- AC4 (responsiveness): the 18 large-review rows pass enforced on an
  optimized tree; the reviewer's own selection is never queued behind more
  than one preparation step.
- AC5 (tests): prepared-file open latency, cancellation, budget caps and the
  large-review window each have a test that fails before the change.

## Continuation

See the sections below for the measured frontier and next step.
