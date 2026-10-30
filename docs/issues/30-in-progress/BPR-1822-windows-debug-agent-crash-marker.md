---
schema: "implement-rfc.card.v1"
id: "BPR-1822"
title: "Diagnose the Windows Debug agent crash-course marker precondition"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "reservation": "docs/issues/_reservations/BPR-1822--BPR-1822--ad118355745b.json", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
---

# Diagnose the Windows Debug agent crash-course marker precondition

Frozen N4 Windows Debug qualification fails application_agent_crash_recovery
in 55.96 seconds: crash-running exits before kill-ready because the internal
30-second wait does not observe both process-id and descendant-id markers.
The later kill, lifetime, lease, journal, recovery and reopen assertions are
unreached. Preserve that raw result; it is not a dedicated timing omission.

## Acceptance

- AC1: Bind the exact current driver, canary, supervisor, product libraries,
  source, flags, command and environment to the marker precondition. Compare
  BPR-1259 and the final selected v0.4.0 bounded recovery receipts, retaining
  the failed intermediate recovery as well. Matching first predicate or source
  bytes does not prove the current cause or current acceptance.
- AC2: At a quiet PC boundary after the pending BPR-1818/BPR-1820 native work,
  run at most one controlled test-only diagnostic or bounded semantic recovery
  when the exact bindings and capacity admission support it. Preserve all
  original semantic assertions, source/command/containment guards and cleanup.
  Any larger Debug wait must be explicit, bounded and provide no timing credit;
  no product deadline, protocol, canary pacing or generic waiver change.
- AC3: Retain the original failure and every unreached predicate separately.
  Accept only complete native semantic markers and cleanup/liveness checks,
  or record the precise remaining defect without a passing-original claim.

## Initial source and historical evidence

The maintained driver, canary, polling header and crash Python wrapper are
compared by exact Git blobs against published040 producer094 and frozen N4.
The best-effort BPR-1259 timer-resolution calls remain present. The prior
240-second test waits / 600-second outer semantic recovery completed the
crash course in 124.2708 seconds, after an intermediate wrong-image selection
still failed. That history is not a proven cause or a present-day PASS.
Exact comparisons and receipts are external in
`BPR-1822-historical-source-comparison-r1.json`. No current native repeat,
source repair, product deadline or protocol/pacing edit has been made.
