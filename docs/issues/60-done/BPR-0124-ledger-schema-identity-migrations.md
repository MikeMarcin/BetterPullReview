---
schema: "implement-rfc.card.v1"
id: "BPR-0124"
title: "Ledger schema, strong identity scoping, and forward-only migrations"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: ["BPR-0120"]
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

## Outcome

The ledger is a separate database file from the epoch catalog, because deleting
private review state must not destroy the derived git cache. It shares the
catalog's connection policy rather than carrying its own.

### Sharing the policy required extracting it first

`configure_connection` lived in `catalog_database.cpp`'s anonymous namespace,
reachable only by the epoch catalog, so a second store could only have copied
it — and a copied policy drifts until the two databases are hardened
differently. It moved to `sqlite_connection.{hpp,cpp}` as a pure move, in its
own commit, with every relocated body diffed against the original.

The move also uncovered that the library initialisation was catalog-private.
The build sets `SQLITE_OMIT_AUTOINIT`, so `sqlite3_initialize` must run before
any other call, and `configure_connection` never did it — the catalog's open
path did, separately. Opening the ledger without it did not fail cleanly; it
jumped through a null pointer. `sqlite_runtime_ready` is now shared and both
stores initialise through it.

### The schema

Version 1 holds `ledger_meta`, `review_scope`, and `file_receipt`; version 2
adds `conversation_receipt`. Current is **2**, deliberately not 1: a migration
mechanism whose only version is the initial one has never migrated anything and
would first be exercised on a user's database. The test builds a real version-1
database and migrates it, then asserts a row written under version 1 survives.

Tables are `STRICT` and identities carry `length(...) = 32` checks, so a wrong
type or a truncated digest fails at the row that caused it rather than becoming
a puzzle read back later.

Every row is scoped by host, account, repository, and pull request. The test
uses pull request 7 on four identities that differ in exactly one field each;
anything shorter than the full scope collides.

### Two findings worth recording

**A fabricated constant.** The schema stamped `application_id = 1112558148`
while validation expected `0x42505244`, which is 1112560196. The decimal was
written out rather than computed. The stamp is now derived from the constant
with `std::to_string`, so the two cannot disagree.

**A vacuous assertion, caught by breaking it.** The forward-only test asserted
only that a newer database is refused with `schema_mismatch` — and it still
passed with the forward-only guard deleted, because `validate` rejected the
version afterwards and produced the same failure. Refusing was never the
property worth proving; refusing *before writing* is. The test now reopens the
refused file and asserts `ledger_meta.schema_format` is untouched. With the
guard removed it reads 99 instead of 2.

### Non-vacuity

Four behaviours were broken separately and each failed its own assertion, then
restored to green:

- Scope lookup reduced to the pull request number — "a reused pull request
  number never shares a scope across hosts, accounts, or repositories".
- The forward-only guard removed — "a refused open leaves the recorded schema
  format untouched", reporting 99.
- The version 1-to-2 migration step skipped — the ledger no longer opens.
- A `scroll_offset REAL` column added to `file_receipt` — "no ledger column
  stores a presentation coordinate", naming `file_receipt.scroll_offset`.
