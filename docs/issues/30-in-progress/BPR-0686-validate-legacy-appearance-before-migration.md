---
schema: "implement-rfc.card.v1"
id: "BPR-0686"
title: "Validate legacy appearance before migration"
status: "In Progress"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0680"]
---

# Validate legacy appearance before migration

## Scope

Independent Settings review found that schema3 migration replaces the old
appearance table and stamps metadata before validating it. Version2 with
schema_format=99 can therefore be normalized and accepted; missing metadata
can fail only after the migration commits. Validate supported legacy identity,
metadata and structural shape before destructive migration; failed admission
must leave the original schema and content unchanged.

Owned paths: SQLite appearance database and focused appearance storage tests.
Routing: standard | Reason: default implementation. Use Astra low per updated routing.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

## Acceptance

AC-05: canonical v1/v2 still migrate/reopen and preserve settings. Future or
missing metadata and malformed legacy schema refuse before migration, retaining
version, rows and columns. Transaction failures roll back. No other store or
credential is touched. Add discriminating direct SQLite cases and run the
focused storage target and canonical tidy for owned changed TUs.

## Evidence

Confirmed by primary and medium-effort reviewer source inspection during BPR-0683.
Not yet repaired or verified.
