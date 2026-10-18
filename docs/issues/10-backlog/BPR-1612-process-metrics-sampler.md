---
schema: "implement-rfc.card.v1"
id: "BPR-1612"
title: "Process-metrics sampler with foreground/background attribution"
status: "Backlog"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1607"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# Process-metrics sampler with foreground/background attribution

## Scope and acceptance

Implement RFC-BPR-0086 "Energy" sampling and AC08.

- A platform adapter behind an application port: process CPU everywhere, and
  wakeups where the platform reports them (macOS `proc_pid_rusage`).
  "Not reported on this platform" otherwise.
- 30 s foreground and 60 s background sampling, split at each transition.
  Invalid deltas are dropped.
- Child `gh` and `git` CPU recorded at reap and attributed to the state active
  at launch.
- Retention: 2 h raw, 48 h of 5-minute rollups, 93 days daily.
