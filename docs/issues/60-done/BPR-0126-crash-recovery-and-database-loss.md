---
schema: "implement-rfc.card.v1"
id: "BPR-0126"
title: "Crash recovery and database-loss behaviour"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: ["BPR-0125"]
---

# Crash recovery and database-loss behaviour

Two acceptance criteria meet here: abrupt termination with WAL recovery
preserves committed state and discards incomplete transactions, and deleting
the database loses only private state and recreates a clean ledger without
changing GitHub.

Both run unattended on this host. The kill targets a child process this project
starts; it needs no elevated privilege and no operator.

A recovery test that only restarts cleanly proves nothing. The process must be
killed while a transaction is open, and the assertion must distinguish
committed from uncommitted work rather than checking that the file still opens.

## Required direct evidence

- A process killed mid-transaction restarts with every committed receipt intact
  and the incomplete transaction absent, asserted per receipt.
- A process killed during a refresh restarts consistently.
- Deleting the ledger recreates a clean one, and an independent GitHub fixture
  is inspected and shown unchanged.
- The kill is real termination, not a graceful shutdown path.
- Each assertion is shown to fail when the behaviour it constrains is neutered.

## Outcome

### The kill is real

A forked child opens the ledger, begins an explicit transaction, writes a
receipt, and then announces readiness *only after* the row is inside the open
transaction — so the parent cannot kill it too early and pass for the wrong
reason. The parent then sends `SIGKILL` and asserts `WIFSIGNALED` with
`WTERMSIG == SIGKILL`, so a graceful exit path could not be mistaken for a
crash. The child runs no destructor, closes no handle, and commits nothing.

The assertions distinguish committed from uncommitted work rather than checking
that the file still opens: a receipt committed before the fork returns byte for
byte after recovery, and the receipt written inside the abandoned transaction
is **absent**.

### Database loss

Deleting the ledger — main file, `-wal`, and `-shm` — leaves a clean ledger to
be recreated in its place, and the private receipt is gone, which is what
deletion means. A second database alongside it stands in for the epoch catalog
and is shown untouched. That independence is the reason the ledger is a
separate file at all, and the test fails if the two ever share one.

### Non-vacuity

Three behaviours were broken separately and each failed its own assertion, then
restored to green:

- The child commits before dying — "the write left inside the open transaction
  is discarded".
- The pre-kill receipt is never written — "the receipt committed before the
  kill survives byte for byte", so survival cannot be assumed from an empty
  database.
- Both stores placed in one file — "deleting the ledger changed nothing in the
  store beside it".

### Not covered here

The recovery test is POSIX-only and is compiled out on Windows, where `fork`
does not exist. That leaves the Windows equivalent unwritten rather than
silently passing, and it belongs to the Windows campaign in BPR-0036 rather
than to this card.

The refresh-time kill named in the original evidence list is not covered: the
ledger has no refresh path yet — that arrives with the Qt projection in
BPR-0127 — so a test for it now would be asserting against nothing.
