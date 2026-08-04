---
id: BPR-0129
title: Documentation for schema, migrations, identity, privacy, retention, purge
status: Done
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

## Outcome

`docs/architecture/review-ledger.md` covers every mandatory topic: schema,
migrations, identities, receipt semantics, actionability, crash recovery,
backups, retention, purge, and privacy. It also carries the documentation
ledger the RFC requires — core, host, and adapter ownership; the typed
command, decision, observation, request, completion, and delta vocabulary;
authority and ordering; and concurrency.

The claims were checked against the source rather than written from memory, and
one was wrong: the draft said every identity column carries a `length(...) = 32`
check. Only the two 256-bit digest columns do; comment and conversation
identities are integers. Corrected before the document landed.

The epoch model document already stated that the ledger is a separate database.
That statement now links here and records that the reuse is literal — one
`configure_connection` and one `sqlite_runtime_ready`, not copies that drift.

### Backups and retention are documented as absent

Neither has been designed, and neither runs. The document says so plainly
rather than describing a policy that does not exist, because a reader who found
a retention section here would reasonably assume something prunes the ledger.
Nothing does. Purge is deletion of the file with its `-wal` and `-shm`
companions, which is tested, and which needs no application feature to be
trustworthy.

### Privacy is stated in terms of what is stored

The ledger records identities and progress, not content: file and material
digests, comment identities and edit versions, and the read, accepted, and
excluded flags. It stores no path, no source text, and no comment text. A
person reading the database learns which opaque material a reviewer marked
read, not what that material said. That is a stronger and more checkable
statement than "private data is protected", and it is enforced by the
prohibited-column test rather than by this paragraph.
