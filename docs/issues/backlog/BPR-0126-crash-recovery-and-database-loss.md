---
id: BPR-0126
title: Crash recovery and database-loss behaviour
status: Backlog
rfc: RFC-BPR-0007
depends_on: [BPR-0125]
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
