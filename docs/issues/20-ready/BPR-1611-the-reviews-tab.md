---
schema: "implement-rfc.card.v1"
id: "BPR-1611"
title: "The Reviews tab"
status: "Ready"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1609", "BPR-1610"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# The Reviews tab

## Scope and acceptance

Implement the RFC-BPR-0086 Reviews presentation and AC07.

- Three ranges, the tiles, the 26-week calendar, "How reviews ended" and
  records. Local-time-zone days, including across a
  daylight-saving change.
- No streak (Q4).
- The two Summary review tiles.
- Load `frontend-design:frontend-design` first.
