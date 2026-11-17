---
schema: "implement-rfc.card.v1"
id: "BPR-2238"
title: "A suggested change fits inside its finding's card"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2236--BPR-2238--777d083d72cc.json","origin":"BPR-2219's look review-report-v002, 2026-11-16"}
---

# A suggested change fits inside its finding's card

## Origin

In the report's Cards look ([[BPR-2219]], docs/design/review-report-v2) the suggested-change box laid over its place in the page runs a few points past the right padding of its finding's bordered box, in light and dark.

## Scope and acceptance

- **AC-a.** A code box and a suggested change stand within the finding's box, with the box's padding at both sides, at every pane width.
- **AC-b.** `qt_agent_report_view` measures it.
