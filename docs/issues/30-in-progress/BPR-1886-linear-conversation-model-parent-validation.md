---
schema: "implement-rfc.card.v1"
id: "BPR-1886"
title: "Validate conversation-model ancestry without repeated walks"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1886--BPR-1886--6d72c0e26776.json","related_cards":["BPR-1882"],"milestone_plan":{"id":"responsiveness-latency-v3","path":"docs/rfcs/plans/responsiveness-latency-v3/milestone-plan.json"}}
---

# BPR-1886: Validate conversation-model ancestry without repeated walks

The owner requested continued direct responsiveness work, preserving other
behavior and landing each verified round on main with a card and journal line.
BPR-1882 made host projection linear; the Qt model separately walks every
parent chain on every apply, including unchanged refreshes on the UI thread.

## Acceptance

- AC1: Resolve the model's ancestry once per update; preserve duplicate, missing,
  self-parent and cyclic-parent handling, including every descendant of a cycle.
- AC2: Preserve exact row order, incremental signals and persistent indexes;
  run the existing generated-model oracle and focused composed/model checks.
- AC3: Record optimized before/after timings with no Debug timing gate.
- AC4: Pass focused source/documentation checks, record inherited failures,
  complete the journal/card and land the round on main.

## Evidence

Pending implementation and verification.
