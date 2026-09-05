---
provisional_id: PACK-BPR-0022-09
title: Correlate separate RFC-BPR-0008 publication with local composition and server identity
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-05, PACK-BPR-0022-08]
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
- Public body bytes contain exactly reviewer-approved text with no hidden or mandatory provenance marker.

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

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
