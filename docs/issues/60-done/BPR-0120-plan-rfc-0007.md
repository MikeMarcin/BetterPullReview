---
schema: "implement-rfc.card.v1"
id: "BPR-0120"
title: "Plan RFC-BPR-0007 verification and allocate its cards"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: []
---

# Plan RFC-BPR-0007 verification and allocate its cards

RFC-BPR-0007 adds the local review ledger: a versioned SQLite store of file and
conversation receipts, notes, exclusions, and local defect metadata, plus the
derived unread, changed-since-review, needs-verification, and next-actionable
projections.

## Outcome

`docs/verification/RFC-BPR-0007.md` records the architecture allocation, what
is reused rather than rebuilt, the eleven-row claim matrix, the deferred-gate
finding, and the card allocation.

The dependency position is clear: `depends_on` names RFC-BPR-0003 and
RFC-BPR-0005, both implemented and verified on available targets, both still
`Proposed` because closure is ordered behind RFC-BPR-0002's Windows evidence.
Done BPR-0088 authorises implementation to advance on available targets, and
closure order is untouched.

**This RFC has no deferred gate of its own.** Its text names no owner-attended
requirement, no Windows-native requirement, no GitHub mutation, no credential,
and no network service, and its non-goals exclude cross-machine
synchronisation. The process-kill recovery criterion kills a child process this
project starts, so it needs no operator and no elevated privilege. Every slice
is unattended work on this host.

Two prohibitions are carried into the schema rather than left to review: no
credentials or duplicated source bodies, and no presentation coordinates — no
Qt index, visual row number, ribbon path, measured y-coordinate, or comparison
scroll position. BPR-0124 asserts these against the created schema, because a
rule enforced only by reading insert statements is a rule that decays.

The ledger reuses `catalog_database.cpp`'s connection policy — `PRAGMA
user_version`, WAL, foreign keys, prepared statements, one serialized writer —
rather than introducing a second one.

Cards BPR-0121 through BPR-0130 are allocated in `backlog/`; `NEXT_ID` is
`BPR-0131`.
