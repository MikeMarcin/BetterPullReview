---
schema: "implement-rfc.card.v1"
id: "BPR-1497"
title: "Bump the diff algorithm revision for linked xdiff"
status: "In Progress"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1485"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r3","source":"Owner answer 2026-10-13 (\"Sure\") to bumping supported_diff_algorithm_revision after BPR-1485 adopted linked xdiff; release-critical for 0.2.0"}
---

# BPR-1497: Bump the diff algorithm revision for linked xdiff

[BPR-1485](../60-done/BPR-1485-adopt-linked-xdiff-as-the-primary-line-matcher.md)
replaced the first-party primary matcher with linked xdiff and kept
`supported_diff_algorithm_revision` at 1 pending the owner. On 2026-10-13 the
owner answered "Sure" to bumping it. xdiff aligns differently, so a result
computed by the old engine must not be served as a current one. Stored review
data (drafts, line comments, private notes and threads, file receipts, tours,
Review Cycle records) must stay usable: re-anchored, migrated or clearly
marked, never silently lost or corrupted.

## Acceptance

- **AC1:** Every store and cache that holds an identity derived from the
  algorithm revision is inventoried, with what happens to it at revision 2.
- **AC2:** `supported_diff_algorithm_revision` is 2. A comparison key or a
  computed result at revision 1 is refused by the diff transition, the diff
  host, the epoch host's selected-diff context and result validation, so it is
  recomputed rather than served; the Review Cycle comparison identity carries
  revision 2.
- **AC3:** Stored review data written under revision 1 keeps matching after
  the bump, with tests that fail on the code before it; no data is lost and no
  user-visible behaviour gets worse.
