---
schema: "implement-rfc.card.v1"
id: "BPR-0857"
title: "sqlite_anonymous_epoch_authority's v1 downgrade misses the append_residency table"
status: "In Progress"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-0851-sweep full ctest sweep; reproduced on macOS by dsp-004; scouted","found_at":"a1864fa296ce3450ce0e0470f36bd0d95cd59470","run":"run-20260916-stations","dispatch":"dsp-004"}
---

# BPR-0857: sqlite_anonymous_epoch_authority's v1 downgrade misses the append_residency table

## Defect

`sqlite_anonymous_epoch_authority` fails with
`FAIL: a v1 catalog opens through the migration`. It fails on Linux at
`a1864fa2` (`docs/verification/dsp4-linux-confirmation.md`) and on macOS
at `20d99914` (`build/r2-logs/dsp4-linux-sweep-mac-classify.log`).

The assertion is at `tests/sqlite_contract/anonymous_epoch_authority_tests.cpp:414`.
Its `downgrade_to_v1` helper (~248-257) drops only the five older tables.
`654235fe` ("Persist and inspect bounded sparse Git residency", 2026-09-13)
did three things:

- raised the catalog `UserVersion` to 7;
- added `append_residency`;
- updated `epoch_catalog_legacy_fixture.hpp` to drop that table.

It did not update this helper. After the `v1` migration, schema validation
(`catalog_database.cpp` ~1060) sees the leftover table as an extra object and
returns `schema_mismatch`.

The scout's confidence is high that this is the cause, and medium that it is
the only one. It did not run the test.

## Scope

- Drop `append_residency` in `downgrade_to_v1`. The helper is used at ~388
  and ~478.
- Confirm that no other schema object from versions 2-7 is left behind.

## Acceptance

- The row passes on macOS and Linux. `sqlite_epoch_catalog_storage` still
  passes.

## Fix (dsp-004, Mac)

Two stale pieces, both left behind by `654235fe` when it raised the catalog to
version 7:

- **The downgrade.** `downgrade_to_v1` now drops `append_residency` first, the
  same order as `epoch_catalog_legacy_fixture.hpp`.
- **The version assertions.** Three assertions still expected version 6 after
  the migration. They now expect 7: `the migrated catalog reports the v7
  version everywhere` and `the retried migration completes`. An `rfc-reviewer`
  found this second cause before the build.

The reviewer also compared the helper against `validate_schema` at version 2.
The six dropped tables are exactly the ones version 2 does not expect. No
other index, trigger or view is left behind.

`sqlite_anonymous_epoch_authority` and `sqlite_epoch_catalog_storage_contract`
pass on macOS (`build/r2-logs/dsp4-int3-ctest.log`). Linux still has to run
them.
