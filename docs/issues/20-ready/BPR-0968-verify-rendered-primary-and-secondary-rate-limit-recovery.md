---
schema: "implement-rfc.card.v1"
id: "BPR-0968"
title: "Verify rendered primary and secondary rate-limit recovery"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0968-rate-limit","path":"docs/rfcs/plans/bpr0968-rate-limit/milestone-plan.json"}}
---

# Verify rendered primary and secondary rate-limit recovery

## Transferred obligation

Original RFC0013 acceptance row 4's full rendered backoff course.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

Provider/pager and BPR-0161 scheduler evidence covers bounded backoff, Retry-After, cancellation, exact session identity and typed states. It does not establish the complete rendered primary/secondary rate-limit workflow for a large review. RFC0013 r4 accepts that bounded component evidence and transfers this unobserved public course without inventing a pass.

## Acceptance

- AC1: Drive primary exhaustion and secondary throttling through the real composition using a safe local provider/transport fixture; observe useful distinct visible states and bounded scheduling, without contacting a production account.
- AC2: Verify Retry-After, capped retries, explicit retry/cancel, authority replacement and recovery preserve exact selection and do not produce request storms or stale publication.
- AC3: Record the exact candidate, input/request counts, rendered state and timing evidence; demonstrate the course fails if backoff or state projection is removed.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.
