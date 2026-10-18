---
schema: "implement-rfc.card.v1"
id: "BPR-1609"
title: "Shared charts, the GitHub tab and the Summary budget bar"
status: "Ready"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1608"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# Shared charts, the GitHub tab and the Summary budget bar

## Scope and acceptance

Implement RFC-BPR-0086 "Charts" (AC02) as a shared Qt chart component,
then the GitHub tab (AC04) and the Summary budget bar and Problems tile (AC03).

- Lanes, maximum labels, the foreground/background stack, gaps, hover with
  hysteresis, edge flipping, theme roles and accessible descriptions. The
  calendar mode is added by whichever of BPR-1611 and BPR-1613 lands
  first.
- Load `frontend-design:frontend-design` first. The owner compares the charts
  with AllTheThings' Energy chart in an attended review.
