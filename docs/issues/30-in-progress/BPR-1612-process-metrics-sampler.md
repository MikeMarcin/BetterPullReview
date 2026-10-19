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
- Where supported Qt/platform APIs provide exact final child counters, record
  `gh` and `git` CPU at reap under the state active at launch. Otherwise retain
  absence and show “Not reported”; no estimates or process-wide deltas.
- Keep official pinned Qt binaries. Unsupported exact child CPU is deferred
  under RFC owner amendment r2 and does not block this pack's closeout.
- Retention: 2 h raw, 48 h of 5-minute rollups, 93 days daily.


## Available implementation and verification

Source `263f45ca91d6b142f80a99c9d206882ac9baeb6a` implements native process CPU, macOS wakeups, always-on30/60s
sampling, activation splitting and shutdown flush. Invalid deltas are dropped.
Schema4 persists separate foreground/background sums and peaks with2h/48h/93d
retention. Host tests retain a background launch through a foreground completion;
the real gh runner also verifies launch-state terminal attribution. Core, host,
SQLite, native Mac, actual gh and public courses pass. Evidence is in the
[RFC ledger](../../verification/RFC-BPR-0086.md) and current continuation.

## Owner amendment r2 and supported capability limits

The owner's 2026-10-18 amendment (mandate `insights-window-v1-r2`) directs
keeping official pinned Qt binaries, ending the Qt-fork approach and deferring
exact child CPU where supported Qt/platform APIs do not provide it. The
[RFC amendment](../../rfcs/RFC-BPR-0086-insights-window.md#owner-amendment-r2-official-qt-and-supported-child-counters)
is authoritative. This capability limit no longer blocks Energy or pack closeout.

Qt6.11.2's Unix reap discards rusage before finished; unsupported gh/git CPU
therefore remains absent. Supervised Git cannot substitute its helper's CPU.
Ordinary Windows children retain an exact process handle through reap; native
Windows evidence is unmeasured. Native Linux qualification is also unmeasured.
The UI shows “Not reported”, preserving measured zero and no-children states.
No Qt patch/build provider, launch wrapper, private symbol, process-wide delta
or sampling estimate is selected. Native own-process CPU, macOS wakeups,
activation splitting, retention and the accepted Energy surface remain intact.
