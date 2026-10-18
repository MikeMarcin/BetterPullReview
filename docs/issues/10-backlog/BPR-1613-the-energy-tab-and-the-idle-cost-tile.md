---
schema: "implement-rfc.card.v1"
id: "BPR-1613"
title: "The Energy tab and the idle-cost tile"
status: "Backlog"
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
- Load `frontend-design:frontend-design` first.
