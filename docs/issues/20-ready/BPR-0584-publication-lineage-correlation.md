---
schema: "implement-rfc.card.v1"
id: "BPR-0584"
title: "Correlate separate RFC-BPR-0008 publication with local composition and server identity"
status: "Ready"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0580","BPR-0583","BPR-0568","BPR-0569"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
---

# Correlate separate RFC-BPR-0008 publication with local composition and server identity

## Objective

Retain local audit lineage after ordinary GitHub publication without modifying public semantics.

## Required final state

- Publish remains a separate draft gesture and traverses the existing proposal/coordinator/adapter/reconciliation path.
- Successful mutation plus reconciliation records exact draft/composition revision, target/head, acting identity, idempotency digest, body digest, ServerCommentId, and reconciled server identity.
- Failed/cancelled/refused attempts are not recorded as successful publication lineage.
- Remote success followed by local persistence failure recovers through idempotency/reconciliation and never emits a duplicate comment.
- Duplicate-source/body/target advisory can resolve local and server lineage without preventing an explicit intentional publish.
- Public prose remains byte-preserved; the RFC-BPR-0022 file representation adds only
  its canonical visible code permalink. Track prose and wire-body digests separately;
  no private provenance or mandatory AI marker enters either representation.

## Non-goals

- A BetterPullReview bot protocol
- Changing GitHub author identity
- Embedding private provenance in GitHub

## Completion evidence

- Mutation trace
- Failure-injection/recovery test
- Designated target or signed recorded fixture
- Independent public-body/author read
- Local lineage lookup by draft and server ID

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

Consume RFC-BPR-0022 logical publication receipts and canonical server identity.
A representation child is never a second publication or a lineage-repair retry.
