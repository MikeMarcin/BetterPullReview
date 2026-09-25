---
schema: "implement-rfc.card.v1"
id: "BPR-0969"
title: "Verify cached-review offline and restart freshness"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0969-offline-inventory","path":"docs/rfcs/plans/bpr0969-offline-inventory/milestone-plan.json"}}
---

# Verify cached-review offline and restart freshness

## Transferred obligation

Original RFC0013 acceptance row 5's composed cached-data offline/restart and visible freshness labels.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

BPR-0161 and current queue tests cover typed offline/recovery and stale-session refusal. Retained Git/catalog history is private exact revision evidence. Neither proves a complete cached-review offline/restart mode with visible freshness labeling. Current RFC status claims no new offline capability.

## Acceptance

- AC1: Inventory which cached review data the actual application can reopen offline, including metadata, source, threads and retained epochs; explicitly distinguish unavailable content from an observed empty result.
- AC2: Exercise network loss and restart through safe real composition, verifying cached/stale labeling and exact account/repository/revision isolation. Reconnection must adopt server truth without silently implying prior freshness.
- AC3: Implement any demonstrated missing required behavior within its owning core/host/Qt boundary, with exact-candidate rendered checks, cancellation and stale-session negative controls; retain all unsupported cases explicitly.

- AC4: As a bounded decomposition of AC2/AC3, isolate same-profile Recent titles, destinations and saved reading by the accepted account through switches and restart, including safe legacy migration; no complete cached-review mode is implied.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.

## Execution record

The bounded [Mac inventory](../../verification/BPR-0969-offline-inventory-2026-09-25.md)
is complete at source `b3197149a74bbb8030a3d4a534e956b11a64ed9f` under
`r4-standalone-offline-inventory-20260925`. Its three-process course demonstrates
retained live snapshot/source after refresh failure, unavailable content after
offline Resume/restart, and unchanged durable Git/epoch identities. The visible
failed-refresh freshness notice has a failing-before control and a successful
changed-server-thread recovery. It adds no offline account authority or durable
PR snapshot. The second and third acceptance criteria remain pending; this card
is unfinished.

Next, diagnose same-profile Recent account-switch title/destination isolation:
those profile-private records currently lack an account key. Then define the
durable snapshot/admission/freshness contract needed for cached-review restart.
Reuse relevant passing evidence with its actual input limits.
