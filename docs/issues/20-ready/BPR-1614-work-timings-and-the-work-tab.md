---
schema: "implement-rfc.card.v1"
id: "BPR-1614"
title: "Work timings and the Work tab"
status: "Ready"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1609"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# Work timings and the Work tab

## Scope and acceptance

Implement RFC-BPR-0086 "Work" (D10) and AC10.

- Main-thread stall detection in the Qt adapter, with the 100 ms threshold,
  count, longest and p95.
- Per-file diff wall time in the application host, and work-limit counts.
- Five slowest files, syntax cache hit rate and evictions, and median source
  search time.
- Load `frontend-design:frontend-design` first.
