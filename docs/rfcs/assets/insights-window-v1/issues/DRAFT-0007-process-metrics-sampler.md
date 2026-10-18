---
schema: "implement-rfc.card.v1"
id: "DRAFT-0007"
title: "Process-metrics sampler with foreground/background attribution"
status: "Backlog"
rfcs: ["RFC-DRAFT-0024"]
depends_on: ["DRAFT-0002"]
platforms: ["mac", "win", "linux"]
---

# Process-metrics sampler with foreground/background attribution

## Scope and acceptance

Implement RFC-DRAFT-0024 "Energy" sampling and AC08.

- A platform adapter behind an application port: process CPU everywhere, and
  wakeups where the platform reports them (macOS `proc_pid_rusage`).
  "Not reported on this platform" otherwise.
- 30 s foreground and 60 s background sampling, split at each transition.
  Invalid deltas are dropped.
- Child `gh` and `git` CPU recorded at reap and attributed to the state active
  at launch.
- Retention: 2 h raw, 48 h of 5-minute rollups, 93 days daily.
