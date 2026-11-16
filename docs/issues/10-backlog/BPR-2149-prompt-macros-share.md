---
schema: "implement-rfc.card.v1"
id: "BPR-2149"
title: "Suggested prompt macros: share through the exchange"
status: "Backlog"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2146"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC24"}
---

# Suggested prompt macros: share through the exchange

RFC-BPR-0107's exchange version 2 carries suggested prompts, with export/import preview and all-or-none apply; version 1 files unchanged.

## Acceptance

RFC-BPR-0125 AC24.

## Hygiene audit 2026-11-15

Verdict: **STILL-VALID**. Not built: agent_configuration_exchange.cpp carries hooks and the skill but no prompt macros (no version 2).
This is a read-only tracker audit: it changes no status, folder or metadata.
