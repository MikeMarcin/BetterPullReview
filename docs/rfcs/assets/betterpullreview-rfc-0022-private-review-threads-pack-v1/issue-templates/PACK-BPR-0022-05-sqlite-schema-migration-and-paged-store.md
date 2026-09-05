---
provisional_id: PACK-BPR-0022-05
title: Implement the next ledger schema, legacy-note migration, durable IDs, and paged private stores
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-03, PACK-BPR-0022-04]
---

# Implement the next ledger schema, legacy-note migration, durable IDs, and paged private stores

## Objective

Create one crash-safe durable authority for private threads, run provenance, lineage, compositions, draft correlation, and publication lineage.

## Required final state

- The next live schema version is allocated without reuse and canonical STRICT table definitions are validated.
- Non-pruned high-water rows allocate every new ID atomically and never scan retained maxima.
- Every supported prior schema migrates forward atomically; exact v5 notes become one-turn threads preserving bytes/attachments and actor=`legacy_unknown`.
- Legacy `local_note` is retired as an authority; any temporary API facade writes only the new authority.
- Paged thread catalog, thread header, turn page, selected provenance, composition, and publication-lineage ports are bounded and cursor-based.
- Durable draft/composition linkage survives restart without a second draft state machine.
- Count/byte/page/total quotas refuse whole with no silent truncation.
- Purge, database deletion, WAL recovery, concurrent opens, future/wrong-app refusal, and process-kill behavior are tested.

## Non-goals

- Loading transcripts through `private_ledger_observation`
- Persisting credentials/source bodies/presentation coordinates
- Backward schema downgrade

## Completion evidence

- Schema inventory
- v1-current migration results
- Exact v5 byte/digest comparison
- Quota/page tests
- Process-kill/restart output
- Database-loss GitHub no-impact evidence

## Documentation impact

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
