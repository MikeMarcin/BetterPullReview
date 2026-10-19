---
schema: "implement-rfc.card.v1"
id: "BPR-1613"
title: "The Energy tab and the idle-cost tile"
status: "Ready"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1609", "BPR-1612"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# The Energy tab and the idle-cost tile

## Scope and acceptance

Implement the RFC-BPR-0086 Energy presentation and AC09.

- Background-only tiles, the stacked chart with the wakeups lane, the 3m
  calendar coloured by background intensity, and the paired fact table.
- Cost per idle hour against the budget line (2 s CPU per background hour, 6 wakeups per minute), with over-budget marking.
- The Summary "Cost while idle" tile.
- Unsupported exact child CPU shows “Not reported”, distinct from zero or no
  children, under RFC owner amendment r2; keep official pinned Qt binaries.
- Load `frontend-design:frontend-design` first.


## Independent presentation evidence

Source `263f45ca91d6b142f80a99c9d206882ac9baeb6a` implements all presentation criteria through the real profile
window and store: background-only tiles and Summary cost, paired facts on every
row, hour/day stack and wakeup lane,93-day local calendar, matched CPU budget
reference and explicit over-budget marking. The seeded public course passes,
including fixed/clamped shell, passive refresh, Light/Dark renders, unsupported
wakeups and a measured zero.1.25x/2x courses pass. `frontend-design` and
`code-simplifier` were read and applied. The accepted titlebar and shared chart
choices are preserved; no additional human criterion is introduced.

The independent criteria retain their passing evidence. Owner amendment r2
conditions child CPU on supported Qt/platform APIs; the unsupported-counter
wording and official-runtime regression checks complete with BPR-1612 before
this card's dependent finish. The [RFC ledger](../../verification/RFC-BPR-0086.md)
and current continuation retain candidate-specific evidence and that dependency.
