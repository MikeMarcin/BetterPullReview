---
schema: "implement-rfc.card.v1"
id: "BPR-2232"
title: "Draft all from the report"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# Draft all from the report

## Origin

The report offers one keep action a finding ([[BPR-2219]]). The Agent tab's Draft all still drafts every finding card of an answer, but the report the reviewer reads has no way to draft the rest at once (design round review-report-v1, B's band).

## Scope and acceptance

- **AC-a.** The report offers drafting every finding not yet drafted, in the Agent tab's Draft all order and by its path.
- **AC-b.** It says how many were drafted and which could not be, as Draft all does.
- **AC-c.** Nothing is published.
