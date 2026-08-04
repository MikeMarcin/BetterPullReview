---
id: BPR-0124
title: Ledger schema, strong identity scoping, and forward-only migrations
status: Backlog
rfc: RFC-BPR-0007
depends_on: [BPR-0120]
---

# Ledger schema, strong identity scoping, and forward-only migrations

The host owns a versioned SQLite ledger holding identities, receipts, rules,
notes, projections, and migration metadata. It reuses the connection policy
already proven in `catalog_database.cpp` — `PRAGMA user_version`, WAL, foreign
keys, prepared statements, one serialized writer and bounded readers. A second
connection policy or a second migration mechanism would be a defect.

Every row is scoped by host, account, repository, pull request, epoch, and
material identity. Reusing a PR number on a different host, or a path in a
different repository, must not surface another identity's receipts.

Credentials and duplicated source bodies are prohibited. So is any presentation
coordinate: no Qt index, visual row number, ribbon path, measured y-coordinate,
or comparison scroll position. Prove that against the created schema rather
than by reviewing the insert statements.

## Required direct evidence

- Every column of the created schema is enumerated and checked; the assertion
  fails if a prohibited column is added later.
- Reusing PR numbers and paths across hosts, accounts, and repositories leaks
  nothing, asserted by reading back through the wrong identity and getting
  nothing.
- A database at every supported prior schema opens and migrates to current.
- Migration is forward-only: attempting to open a newer schema with an older
  binary fails closed rather than corrupting it.
- Each assertion is shown to fail when the behaviour it constrains is neutered.
