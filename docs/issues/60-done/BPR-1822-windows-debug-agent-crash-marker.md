---
schema: "implement-rfc.card.v1"
id: "BPR-1822"
title: "Diagnose the Windows Debug agent crash-course marker precondition"
status: "Done"
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


## Native bounded semantic result

The single controlled current-source Windows standard Debug recovery passed in
127.7493262 seconds on 2026-10-30. It used the unchanged source864f98 product,
canary, supervisor, library and helper inputs, with111 explicit native hash
bindings checked before and after. The diagnostic image SHA256 is
`093bac90d952b56bcee7ed1607116a3dadfde38e9676d69ec5acb63779e5f39a`.
Only two local test wait constants became240000ms; the copied wrapper used a
240-second marker readiness bound and600-second reinvocation/outer bound.
Product deadlines, protocol, pacing, work size and all five-second post-kill
liveness/cleanup bounds stayed unchanged.

The controlled invocation reached both original reopen markers, exact owned
workspace reclamation and the final kill-course PASS. The original identity,
lease, journal, preserved substitute, SQLite/Git integrity and new session
assertions all remained in the maintained wrapper. No negative marker occurred.
The scheduled task is terminal Ready, observed owner/child identities are absent,
the private project/copied test files were removed, and both source checkouts
remain clean. Full receipts,111 input bindings, exact native argv/environment,
raw output and quiescence are retained externally in
`windows-current-native-evidence-r1` and PC `n6-agent-bounded-recovery-r1` under
the release run root.

This accepts complete bounded Debug semantics; it does not pass the original
30-second readiness assertion, infer a proven performance cause, grant timing
credit, change production behavior or erase the original failure and unreached
checks. Future qualification retains its actual raw results; reuse of this
semantic proof requires its exact unchanged input/command bridge. No tracked
product or test timer was changed for this diagnosis.
