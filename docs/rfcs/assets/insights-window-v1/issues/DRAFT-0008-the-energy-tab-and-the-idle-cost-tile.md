---
schema: "implement-rfc.card.v1"
id: "DRAFT-0008"
title: "The Energy tab and the idle-cost tile"
status: "Backlog"
rfcs: ["RFC-DRAFT-0024"]
depends_on: ["DRAFT-0004", "DRAFT-0007"]
platforms: ["mac", "win", "linux"]
---

# The Energy tab and the idle-cost tile

## Scope and acceptance

Implement the RFC-DRAFT-0024 Energy presentation and AC09.

- Background-only tiles, the stacked chart with the wakeups lane, the 3m
  calendar coloured by background intensity, and the paired fact table.
- Cost per idle hour against the budget line (2 s CPU per background hour, 6 wakeups per minute), with over-budget marking.
- The Summary "Cost while idle" tile.
- Load `frontend-design:frontend-design` first.
