---
id: BPR-0128
title: Composition-root wiring and public-path evidence
status: Backlog
rfc: RFC-BPR-0007
depends_on: [BPR-0127, BPR-0126]
---

# Composition-root wiring and public-path evidence

Wire the ledger into the real application and prove the whole path through the
real executable rather than through a harness: a receipt marked in the running
application reaches SQLite and returns as an accepted projection, and survives
a restart of that application.

At most one application instance runs at a time.

## Required direct evidence

- The real executable marks a receipt, and the state is read back from the
  database file the application actually used.
- Restarting the real application shows the state persisted.
- The public-path markers report the ledger stage explicitly, so a regression
  names the stage rather than failing anonymously.
- Each assertion is shown to fail when the behaviour it constrains is neutered.
