---
schema: "implement-rfc.card.v1"
id: "BPR-0969"
title: "Verify cached-review offline and restart freshness"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693"}
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

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.
