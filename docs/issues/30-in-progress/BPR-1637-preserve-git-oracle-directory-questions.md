---
schema: "implement-rfc.card.v1"
id: "BPR-1637"
title: "Preserve Git oracle directory questions"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1637: Preserve Git oracle directory questions

Ubuntu's actual harness failure is C5 `ignorecase-true` subject BUILD: Git says
kept, K says ignored. The fixture's BUILD/ becomes `{Path: BUILD, Directory:
true}` for K, but the oracle serializer drops Directory. On a case-sensitive
filesystem bare BUILD is absent and Git treats it as a file; on the Mac it
aliases the existing build directory, hiding the mismatch in the questions.
A disposable missing-directory control confirms with Apple Git2.50.1 that bare
BUILD is kept while BUILD/ matches build/. The owned root is removed.

## Acceptance

- AC1: Supply the same directory type to both readers through fixture construction before all queries/snapshots; keep original Git query bytes, check-attr input, product policy, full equality and exact existing dispositions unchanged.
- AC2: Add a corpus regression whose explicit directory starts absent before preparation, retain its original failure, then verify prepared type and a passing affected harness; preserve native Linux rerun after its suite is quiescent.
- AC3: Pass focused format/tidy/architecture/documentation/diff checks, self-review and commit explicit paths, account for owned commands and hand remaining full gates to the coordinator.

## Counterexample and corrected boundary

The missing-directory negative fails on the Mac with the original query, proving
the typed question mismatch independently of filesystem case folding. The first
serializer attempt appends slash to every directory and repairs that negative,
but the complete harness correctly rejects eight new logs/neg comparisons:
`logs/` matches `logs/**`, while the directory path `logs` itself is kept.
That attempt is rejected; no disposition changes. The coordinator agrees the
faithful boundary is fixture construction: materialize declared directory extras
before all queries and snapshots. The new `explicit-directory-query` case
asserts its path starts absent before that common preparation, then both readers
receive the same actual directory with the original bare Git query. The original
negative and eight counterexamples remain retained in ignored receipts.
