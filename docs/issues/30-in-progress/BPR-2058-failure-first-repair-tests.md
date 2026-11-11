---
schema: "implement-rfc.card.v1"
id: "BPR-2058"
title: "Prioritize previously failed repair qualification tests"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-2057"]
priority: "P1"
---

# BPR-2058: Failure-first repair qualification

## Scope

The owner's final direction, 2026-11-11, drops circular rotation as needless
complexity. Preserve the runner's existing collect-all behavior, prioritize
previously failed tests, then run the remaining tests with normal CTest scheduling.
Prior results are ordering hints only, never reused passing evidence. Keep current
selected coverage and fixture/dependency groups. Apply only to future main work;
the active v0.4.2 release and its frozen procedure remain unchanged.

## Acceptance criteria

- AC1: Prior structured same-preset failures run first with their full fixture and
  dependency groups, followed by all remaining selected rows using native scheduling.
- AC2: Collect all results, including remaining rows after a repeated early failure.
  Preserve complete coverage, raw per-batch reports, failed/missing/skipped rows,
  cleanup and locks. Refuse excluded/unknown prior failures rather than hiding them.
- AC3: Focused real CTest controls, documentation and independent review pass;
  the active release remains unchanged and no benchmark campaign is run.
