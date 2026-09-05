---
schema: "implement-rfc.card.v1"
id: "BPR-0581"
title: "Capture requested and protocol-reported runtime provenance for each agent run"
status: "Backlog"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0579"]
milestone: "Post-Pack v2"
---

# Capture requested and protocol-reported runtime provenance for each agent run

## Objective

Extend the existing invocation/protocol/result path with honest bounded run metadata while preserving agent credential and process boundaries.

## Required final state

- Invocation requests can carry bounded optional requested model/effort settings without turning profile names into runtime claims.
- Each dispatch receives one run identity and snapshots profile/protocol/binding/entry point/private source IDs.
- Protocol adapters expose model/effort/tool version/remote IDs only when genuinely reported; App Server remote session/thread/turn data is surfaced as typed metadata.
- External-terminal and unsupported protocols cleanly report unknown.
- Requested and reported disagreement survives as two facts and a typed conflict.
- No prose/executable/profile-name scraping or provider credential capture exists.
- Protocol and process fixtures cover full, partial, conflicting, malformed, oversized, stale, cancelled, and unknown metadata.

## Non-goals

- Calling provider APIs directly
- Guaranteeing every agent protocol reports model or effort
- Storing raw environment values

## Completion evidence

- Per-protocol capability matrix
- Decoder/result fixture tests
- Child argv/env/IPC canary
- Secret scan of stored/evidence data

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
