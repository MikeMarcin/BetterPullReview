---
schema: "implement-rfc.card.v1"
id: "BPR-2231"
title: "A report's Drafted and Kept marks are stored and shared with the Agent tab's finding cards"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# A report's Drafted and Kept marks are stored and shared with the Agent tab's finding cards

## Origin

What a report's keep action made (Drafted, Kept) is remembered by the report view for the session, by the report's turn ([[BPR-2219]]). It is lost at restart, and the Agent tab's finding card for the same finding does not know of it, so one finding can be drafted twice from two places.

## Scope and acceptance

- **AC-a.** A finding drafted or kept from the report still reads so after a restart.
- **AC-b.** A finding drafted from the report reads Drafted on its Agent tab card, and the other way round.
- **AC-c.** The state is the turn's one state for that finding, decided in core, not kept twice.
