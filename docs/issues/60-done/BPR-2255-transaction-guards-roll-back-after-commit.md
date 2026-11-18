---
schema: "implement-rfc.card.v1"
id: "BPR-2255"
title: "Every launch logs a storage_statement sql_error, because three stores' transaction guards roll back after COMMIT"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2255--BPR-2255--2b40e7eee49d.json","origin":"seen while verifying BPR-2254, 2026-11-18; the coordinator asked for its store and statement","related_cards":["BPR-1831","BPR-2254"]}
---

# BPR-2255: Every launch logs a storage_statement sql_error, because three stores' transaction guards roll back after COMMIT

## Observed

Every launch of 0.5.0 build 66 and of the BPR-2254 candidate, a fresh profile
included, logs:

```text
storage.database.failed action=storage_statement failure=sql_error store=unknown
```

## Cause

A temporary trace in the storage diagnostics' SQLite error log (its message
and a backtrace; never committed), on the dev app at `ba6a84c54` run
offscreen in a scratch home, showed four errors on a fresh profile's first
launch, each `statement aborts at 1: cannot rollback - no transaction is
active; [ROLLBACK]`:

| Store | Guard | Where |
|---|---|---|
| profile registry | `registry_transaction` | `establish` and `save` |
| window restoration | `transaction_scope` | `establish` |
| remembered connections | `transaction` | `establish` |

Each guard's destructor runs `ROLLBACK` unconditionally. When its scope
committed, no transaction is open, so SQLite fails the statement and logs
it. The guard runs outside any statement the diagnostics noted, so the line
says `store=unknown`, and the aggregator folds the four into one line.
`cached_review_overview_store.cpp` and `review_tour_store.cpp` (`rollback`)
have the same unconditional guard on paths a launch does not reach.

It is not the [[BPR-2254]] class: no `ADD COLUMN`, no virtual table, and
nothing is lost or switched off. Every `COMMIT` before such a guard
succeeded, and a guard whose `COMMIT` failed still has its transaction open
and still rolls it back. The harm is the log: a failure line on every launch
that means nothing, beside which a real one (BPR-2254's `store=agent_setup`
line) is easy to miss.

## Change

- `detail::rollback_open_transaction` rolls back only while a transaction is
  open (`sqlite3_get_autocommit` is 0). The five guards use it.
- `sqlite_storage_diagnostics` opens, loads and saves those stores under a
  diagnostics scope and expects no failed line.

## Acceptance

- AC1. A launch on a fresh profile logs no `storage.database.failed` line.
- AC2. A guard whose scope did not commit still rolls back.
- AC3. A contract test fails if a successful load or save of those stores
  logs a storage failure.

## Evidence (2026-11-18, macOS, Debug dev tree)

Commit `ee027baf3` on `claude/release-0-5-0-rollback-guard`.

- AC1. The dev app at `ee027baf3` plus the same temporary trace, launched
  twice offscreen in a new scratch home: no `ROLLBACK` error reached
  SQLite's error log, and both session logs hold 42
  `storage.database.succeeded` lines and no `storage.database.failed` line.
  The trace's only entries were second-launch `SQLITE_NOTICE_RECOVER_WAL`
  notices (the launcher ends the app with SIGTERM), which the diagnostics
  do not record as failures.
- AC2. `sqlite_storage_diagnostics` saves a profile, has a stale save
  refused inside its transaction, then saves again: the next `BEGIN
  IMMEDIATE` and `COMMIT` succeed, so the refused scope was rolled back.
- AC3. The same row opens, reads and writes the profile registry, window
  restoration and remembered connections under a diagnostics scope and
  expects no failed line. With the five guards reverted to `HEAD~1` it
  fails at "and a launch's reads and writes record no failure".
- Passed: `sqlite_storage_diagnostics`, `sqlite_profile_registry_storage`,
  `sqlite_window_review_restoration_storage`,
  `sqlite_remembered_connection_storage`, `sqlite_cached_review_overview`,
  `sqlite_review_tour_storage`, `sqlite_private_store_open`,
  `architecture_boundaries`; clang-format 22 check and clang-tidy clean on
  the changed files; `python3 scripts/verify_documentation.py --root .`.

