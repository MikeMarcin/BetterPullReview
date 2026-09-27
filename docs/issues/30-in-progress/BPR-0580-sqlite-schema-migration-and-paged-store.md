---
schema: "implement-rfc.card.v1"
id: "BPR-0580"
title: "Implement the next ledger schema, legacy-note migration, durable IDs, and paged private stores"
status: "In Progress"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0578","BPR-0579"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
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

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Resolve and test the live RFC's 1 MiB body versus 512 KiB page contract with
explicit immutable body continuations, progress, and no silent truncation.
Recheck total invocation-input bounds for selected compositions. Ledger schema
6 and recovery schema 5 are now separate live authorities; recheck each frontier
before allocation. Preserve exact v5 migration cases plus v6 note revision/CAS
and high-water behavior.
