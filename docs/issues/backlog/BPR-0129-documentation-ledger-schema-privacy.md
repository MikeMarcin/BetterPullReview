---
id: BPR-0129
title: Documentation for schema, migrations, identity, privacy, retention, purge
status: Backlog
rfc: RFC-BPR-0007
depends_on: [BPR-0128]
---

# Documentation for schema, migrations, identity, privacy, retention, purge

The RFC makes schema, migration, identities, receipt semantics, actionability,
backups, retention, purge, crash recovery, and privacy mandatory documentation.

The documentation ledger must identify functional-core, application-host, and
Qt-presentation-adapter ownership; typed intents, events, observations,
requests, and deltas; authority; concurrency and generation; and any
evidence-backed no-impact region.

Privacy is the part most easily written as a good intention. State what the
ledger stores, what it refuses to store, and how a user removes it.

## Required direct evidence

- Every mandatory topic is present and matches the implementation, checked
  against the schema rather than against the design intent.
- The prohibited-content rule names credentials, duplicated source bodies, and
  presentation coordinates, and points at the test that enforces it.
- `verify_documentation.py` passes and the documentation-structure and
  issue-graph contracts hold.
