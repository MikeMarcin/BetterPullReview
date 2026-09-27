---
schema: "implement-rfc.card.v1"
id: "BPR-0580"
title: "Implement the next ledger schema, legacy-note migration, durable IDs, and paged private stores"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0578","BPR-0579"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"Next schema and canonical STRICT validation","outcome":"pass","revision":"25fd32efd8ca316ca3473f9a6a928265eb0d6000","evidence":{"kind":"observation","detail":"Ledger v7 inventory and all-version canonical/wrong-app/future checks pass. Direct commands and limits: docs/verification/RFC-BPR-0023.md M4."}},{"criterion":"Non-pruned atomic high-water allocation","outcome":"pass","revision":"25fd32efd8ca316ca3473f9a6a928265eb0d6000","evidence":{"kind":"observation","detail":"Restart, purge, exhaustion, CAS and simultaneous allocation race pass without retained-maximum allocation. Direct commands and limits: docs/verification/RFC-BPR-0023.md M4."}},{"criterion":"Every prior origin and exact v5 migration","outcome":"pass","revision":"25fd32efd8ca316ca3473f9a6a928265eb0d6000","evidence":{"kind":"observation","detail":"v1-v6 ledger course and exact NUL/Unicode/attachment/digest/legacy actor migration pass; v6 CAS/high-water 99 becomes 100. Direct commands and limits: docs/verification/RFC-BPR-0023.md M4."}},{"criterion":"One private-content authority","outcome":"pass","revision":"25fd32efd8ca316ca3473f9a6a928265eb0d6000","evidence":{"kind":"observation","detail":"local_note authority removed; facade writes immutable canonical revisions and shares CAS with expanded threads. Direct commands and limits: docs/verification/RFC-BPR-0023.md M4."}},{"criterion":"Bounded paged private ports","outcome":"pass","revision":"25fd32efd8ca316ca3473f9a6a928265eb0d6000","evidence":{"kind":"observation","detail":"64-row header/revision/dependency/receipt pages, indexed keysets, exact 1 MiB body continuations, bounded run/source reads and stale refusals pass. Direct commands and limits: docs/verification/RFC-BPR-0023.md M4."}},{"criterion":"Existing durable draft and composition linkage","outcome":"pass","revision":"25fd32efd8ca316ca3473f9a6a928265eb0d6000","evidence":{"kind":"observation","detail":"Core-created composition/draft atomic commit, restart and process kill preserve exact body/target/link; exact-copy actor and late terminal regressions pass. Direct commands and limits: docs/verification/RFC-BPR-0023.md M4."}},{"criterion":"Whole transaction quota refusal","outcome":"pass","revision":"25fd32efd8ca316ca3473f9a6a928265eb0d6000","evidence":{"kind":"observation","detail":"All counter boundary fixtures, real 4096-thread and 64 MiB body saturation, page/input limits and allocation rollback checks pass. Direct commands and limits: docs/verification/RFC-BPR-0023.md M4."}},{"criterion":"Purge loss WAL concurrency and process kill","outcome":"pass","revision":"25fd32efd8ca316ca3473f9a6a928265eb0d6000","evidence":{"kind":"observation","detail":"Dependency cleanup, fresh-incarnation loss, wrong-app/future and WAL tests pass; actual POSIX kill before/after thread/draft/publication commits and migration rollback/retry pass. Synthetic shared-authority isolation only; later designated-publication/native acceptance retained. Direct commands and limits: docs/verification/RFC-BPR-0023.md M4."}}]
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
