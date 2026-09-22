---
schema: "implement-rfc.card.v1"
id: "BPR-0972"
title: "Verify application-wide active-set and changed-set scaling"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693"}
---

# Verify application-wide active-set and changed-set scaling

## Transferred obligation

Original RFC0013 architecture-instrumentation acceptance row 9 beyond recorded bounded paths.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

Existing diff index/cache/history, host identity and Qt loading/selection evidence demonstrates bounded behavior at named paths. It does not prove blanket active-set/changed-set scaling over the entire application. BPR-0940 separately retains composed GitHub/window layout failures; this card must not treat green internal helpers as whole-application proof.

## Acceptance

- AC1: Map routine selection, receipt, conversation, queue, overview, epoch and source operations to actual public entry points and retained owners. Identify full scans, resets, synchronous I/O and untagged completions with concrete evidence.
- AC2: Add focused structural counters or direct negative controls only where current evidence can miss a named claim; verify bounded updates scale with active plus changed data, completion is serialized and stale/cancelled work cannot publish.
- AC3: Exercise representative work under concurrent background loading and after close/reopen; report exact source, data sizes, visited records, allocation/reset counts and platform limits, with BPR-0967 owning hardware latency/memory budgets.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.
