---
schema: "implement-rfc.card.v1"
id: "BPR-1612"
title: "Process-metrics sampler with foreground/background attribution"
status: "In Progress"
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


## Available implementation and verification

Source `263f45ca91d6b142f80a99c9d206882ac9baeb6a` implements native process CPU, macOS wakeups, always-on30/60s
sampling, activation splitting and shutdown flush. Invalid deltas are dropped.
Schema4 persists separate foreground/background sums and peaks with2h/48h/93d
retention. Host tests retain a background launch through a foreground completion;
the real gh runner also verifies launch-state terminal attribution. Core, host,
SQLite, native Mac, actual gh and public courses pass. Evidence is in the
[RFC ledger](../../verification/RFC-BPR-0086.md) and current continuation.

## Remaining blocker

Exact child CPU at reap is **not satisfied** on Unix. Qt6.11.2 internally calls
`forkfd_wait(..., nullptr)`, discarding rusage before `finished`. libproc can read
a Mac zombie but cannot recover it after reap. The existing synchronous wait
paths poll and invoke death handling directly, bypassing a QSocketNotifier event
filter. Process-global RUSAGE_CHILDREN deltas would mix simultaneous lanes and
are not used. No launch wrapper, ownership change or private-symbol hook is added.
A qualified exact-child observation seam is the next engineering action.

Ordinary Windows children retain an exact process handle through reap, but native
Windows evidence is pending. Supervised Git cannot report its helper's CPU as
Git CPU. Unavailable child counters remain explicit in the UI and store. Native
Linux qualification is unmeasured. These are engineering obligations; no owner
manual action or human judgment is requested.
