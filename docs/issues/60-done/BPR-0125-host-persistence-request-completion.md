---
id: BPR-0125
title: Host persistence request, typed completion, transactions, and WAL
status: Done
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

## Outcome

`review_ledger_application_host` turns a receipt command into durable state and
a published projection, in that order. `review_ledger_port.hpp` defines the
store and projection interfaces, so the host is exercised against a fake and
the real database is exercised separately.

### The ordering is observed, not assumed

The fake store and the fake projection append to **one shared log**, so
"published after committed" is something the test reads rather than something
the code is trusted to do. A successful command produces exactly
`request, committed, published`; a failing one produces
`request, commit-refused, failed`.

Publishing before the commit would show a reader a state the database does not
hold — and it would survive precisely until the next restart contradicted it.

### Three states that are easy to conflate

A command the **core** refuses never reaches the store: the log holds only
`failed`, and the failure carries a core rejection with no store reason. A
command the **store** refuses publishes no accepted projection and leaves the
retained receipt at the value the database still holds; adopting the rejected
one would leave the host and the database disagreeing until something else
overwrote it. A command that is legal but **moves nothing** produces no
request, no commit, and no publication at all — persisting it would turn every
repeated click into a write.

`receipt_failure_publication` carries the core rejection and the store failure
in one record, with exactly one populated, so a caller never has to guess which
failure vocabulary applies.

A store failure is also shown not to poison the host: the next command
succeeds.

### The real database

Restart survival is proven against a real file rather than the fake. Each of
the four states — seen, read, accepted, excluded — is written to its own file
identity, so a bug collapsing them onto one another cannot hide behind a shared
row, and each returns byte for byte including its material. A file that was
never written reads back **absent** rather than defaulted, so "no receipt" stays
distinguishable from "unread".

Scope isolation is proven at the receipt level too: the same file identity
written under one host carries no receipt under another.

### Non-vacuity

Four behaviours were broken separately and each failed its own assertion, then
restored to green:

- Publishing before committing — "the request precedes the commit and the
  commit precedes the publication".
- Adopting the receipt after a failed commit — "the retained receipt keeps the
  value the database still holds".
- Persisting an unchanged command — "a command that moves nothing produces no
  request, commit, or publication".
- Dropping the scope from the receipt lookup — "the same file identity carries
  no receipt in the other scope".
