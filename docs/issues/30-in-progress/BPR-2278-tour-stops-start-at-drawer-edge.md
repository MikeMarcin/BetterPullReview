---
schema: "implement-rfc.card.v1"
id: "BPR-2278"
title: "A tour's stops start at the drawer's left edge"
status: "In Progress"
rfcs: []
depends_on: ["BPR-2274"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2277--BPR-2279--424d9c5fc0e9.json","found_by":"owner attended review, 2026-11-19, frozen fixture build of 91dedb8a3","related_cards":["BPR-2274"]}
---

# BPR-2278: A tour's stops start at the drawer's left edge

## Observed

Owner, 2026-11-19: "stops are still started off shifted to the right like
they were when there was a parent section, they can start on the left of the
drawer now." [[BPR-2274]] removed the Problem and Solution; the stop route
kept the indentation it had under them.

## Acceptance

- AC1. The stop route (number rail, drag handles, cards, Add stop) aligns
  with the drawer's left content edge, the Draft Tour button.
- AC2. A Qt test asserts the geometry.
- AC3. Focused tests, format and the documentation gate pass.
