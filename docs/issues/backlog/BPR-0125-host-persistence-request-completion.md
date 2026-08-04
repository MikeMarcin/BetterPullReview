---
id: BPR-0125
title: Host persistence request, typed completion, transactions, and WAL
status: Backlog
rfc: RFC-BPR-0007
depends_on: [BPR-0124, BPR-0121]
---

# Host persistence request, typed completion, transactions, and WAL

A receipt command produces a semantic persistence request; the host commits it
and returns a typed completion; only then does the core publish the accepted
projection. The ordering is the contract — a projection published before the
commit would show the user a state the database does not hold.

The host also builds the bounded receipt and conversation observations the core
consumes. It never hands the core the ledger.

## Required direct evidence

- The request, the commit, and the typed completion are separately observable,
  and the accepted projection is shown to follow the completion rather than the
  request.
- A failed commit produces a typed failure and no projection change, leaving
  the previous state intact.
- Concurrent readers see committed state only; the serialized writer is proven
  by a test that would otherwise interleave.
- Read, accepted, excluded, and seen all survive a clean restart identically.
- Each assertion is shown to fail when the behaviour it constrains is neutered.
