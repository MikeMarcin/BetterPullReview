---
schema: "implement-rfc.card.v1"
id: "BPR-0584"
title: "Correlate separate RFC-BPR-0008 publication with local composition and server identity"
status: "Done"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0580","BPR-0583","BPR-0568","BPR-0569"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
acceptance: [{"criterion":"separate-publish-existing-path","outcome":"pass","revision":"1f25799b5f327d6c7fefd2214a8371e0f62ecb2c","evidence":{"kind":"observation","detail":"Actual composition -> persisted begin_publish -> existing coordinator/session/logical recovery uses a separate gesture; acceptance alone produces no lineage or mutation.","refs":["docs/verification/RFC-BPR-0023.md","docs/verification/RFC-BPR-0023-live-audit.md","docs/verification/RFC-BPR-0023.m7-handoff.json","docs/verification/RFC-BPR-0023.m7-amendment.json","docs/architecture/review-round-trip.md","docs/architecture/review-ledger.md","tests/sqlite_contract/private_review_host_tests.cpp","tests/sqlite_contract/private_review_storage_tests.cpp","tests/github_contract/review_comment_permalink_tests.cpp"]}},{"criterion":"exact-successful-lineage","outcome":"pass","revision":"1f25799b5f327d6c7fefd2214a8371e0f62ecb2c","evidence":{"kind":"observation","detail":"Exact revision/composition/actor/target/head/key and prose/wire digests join to the canonical reconciled server ID; draft completion and join commit atomically.","refs":["docs/verification/RFC-BPR-0023.md","docs/verification/RFC-BPR-0023-live-audit.md","docs/verification/RFC-BPR-0023.m7-handoff.json","docs/verification/RFC-BPR-0023.m7-amendment.json","docs/architecture/review-round-trip.md","docs/architecture/review-ledger.md","tests/sqlite_contract/private_review_host_tests.cpp","tests/sqlite_contract/private_review_storage_tests.cpp","tests/github_contract/review_comment_permalink_tests.cpp"]}},{"criterion":"failed-cancelled-refused-not-success","outcome":"pass","revision":"1f25799b5f327d6c7fefd2214a8371e0f62ecb2c","evidence":{"kind":"observation","detail":"Prepare refusal, wrong actor and preflight cancellation send zero POSTs and store no successful lineage. Failed refresh leaves pending only.","refs":["docs/verification/RFC-BPR-0023.md","docs/verification/RFC-BPR-0023-live-audit.md","docs/verification/RFC-BPR-0023.m7-handoff.json","docs/verification/RFC-BPR-0023.m7-amendment.json","docs/architecture/review-round-trip.md","docs/architecture/review-ledger.md","tests/sqlite_contract/private_review_host_tests.cpp","tests/sqlite_contract/private_review_storage_tests.cpp","tests/github_contract/review_comment_permalink_tests.cpp"]}},{"criterion":"remote-success-local-failure-read-only-repair","outcome":"pass","revision":"1f25799b5f327d6c7fefd2214a8371e0f62ecb2c","evidence":{"kind":"observation","detail":"Real competing SQLite BEGIN IMMEDIATE blocks local receipt commit after remote success. Reopen and repeated explicit reconcile_publication repair retain POST count; quota reserve also passes.","refs":["docs/verification/RFC-BPR-0023.md","docs/verification/RFC-BPR-0023-live-audit.md","docs/verification/RFC-BPR-0023.m7-handoff.json","docs/verification/RFC-BPR-0023.m7-amendment.json","docs/architecture/review-round-trip.md","docs/architecture/review-ledger.md","tests/sqlite_contract/private_review_host_tests.cpp","tests/sqlite_contract/private_review_storage_tests.cpp","tests/github_contract/review_comment_permalink_tests.cpp"]}},{"criterion":"duplicate-advisory-and-lookups","outcome":"pass","revision":"1f25799b5f327d6c7fefd2214a8371e0f62ecb2c","evidence":{"kind":"observation","detail":"Bounded advisory returns exact source/body/target server lineage, with draft and server-ID lookup; advisory has no admission or suppression role.","refs":["docs/verification/RFC-BPR-0023.md","docs/verification/RFC-BPR-0023-live-audit.md","docs/verification/RFC-BPR-0023.m7-handoff.json","docs/verification/RFC-BPR-0023.m7-amendment.json","docs/architecture/review-round-trip.md","docs/architecture/review-ledger.md","tests/sqlite_contract/private_review_host_tests.cpp","tests/sqlite_contract/private_review_storage_tests.cpp","tests/github_contract/review_comment_permalink_tests.cpp"]}},{"criterion":"public-prose-and-wire-digests","outcome":"pass","revision":"1f25799b5f327d6c7fefd2214a8371e0f62ecb2c","evidence":{"kind":"observation","detail":"Native exact prose and authorized synthetic canonical visible permalink plus exact prose are separately hashed; fixture JSON reader confirms exact body and actor. Private source bytes remain unchanged.","refs":["docs/verification/RFC-BPR-0023.md","docs/verification/RFC-BPR-0023-live-audit.md","docs/verification/RFC-BPR-0023.m7-handoff.json","docs/verification/RFC-BPR-0023.m7-amendment.json","docs/architecture/review-round-trip.md","docs/architecture/review-ledger.md","tests/sqlite_contract/private_review_host_tests.cpp","tests/sqlite_contract/private_review_storage_tests.cpp","tests/github_contract/review_comment_permalink_tests.cpp"]}},{"criterion":"designated-target-or-signed-recording","outcome":"pass","revision":"3c9f0fc151be8cc9c2b5f333927473d1e34ef0a4","evidence":{"kind":"observation","detail":"Owner-designated BPR-Test PR 1; exact signed Release capsule and one-shot approval. Private revision 16 -> composition 18 -> draft 20 -> separate Publish. Opt-in live CTest passed 1/1; one new review comment; no retry.","refs":["docs/verification/RFC-BPR-0023-live-publication.md","docs/verification/RFC-BPR-0023.md"]}},{"criterion":"independent-external-public-body-actor-read","outcome":"pass","revision":"3c9f0fc151be8cc9c2b5f333927473d1e34ef0a4","evidence":{"kind":"observation","detail":"Independent gh API read returned comment 4116400082/node PRRC_kwDOTyvof871W0fS, exact body, actor MikeMarcin, path/right line 1/head; read-only local publication 21 joins draft 20 and same server node.","refs":["docs/verification/RFC-BPR-0023-live-publication.md","docs/verification/RFC-BPR-0023.md"]}}]
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
