---
schema: "implement-rfc.card.v1"
id: "BPR-1670"
title: "Speed up the native Windows suite without weakening coverage"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"kind":"corrective","origin":"Owner mandate 2026-10-20: Have an implementer look into why the Windows suite takes almost 3 hours and speed it up without weakening the tests.","reservation":"docs/issues/_reservations/BPR-1670--BPR-1670--1dbc02b123e1.json","milestone_plan":{"id":"bpr1670-windows-suite-speedup","path":"docs/rfcs/plans/bpr1670-windows-suite-speedup/milestone-plan.json"}}
---

# Speed up the native Windows suite without weakening coverage

## Mandate and baseline

Mandate revision `windows-suite-speedup-r1`, source baseline `3547e8ef6`.
The retained exact v0.3.1 native Windows pass at `2ed7e5f98641d75678f861dad0edbe658b5f5d22`
completed 784 rows (783 pass, one configured Debug skip), four jobs, in
9563.163747 seconds wall time and 11649.0845232 summed test seconds.
408 serial rows account for 8866.9004852 seconds. Compare matched focused
controls before spending a further full gate; distinguish cold/warm setup.

## Acceptance

- AC1: Explain the measured native Windows wall and summed runtime, serial costs and causal hotspots using retained receipts and matched focused controls.
- AC2: Demonstrate a meaningful native runtime improvement through a behavior-preserving engineering correction, retaining every maintained test, fixture/workload, assertion, budget, timeout, containment boundary, network isolation rule, resource lock and four-job policy; keep GUI/performance courses serial.
- AC3: Run focused positive/adversarial regressions against the exact candidate and one complete comparable native Windows suite, preserving raw failed attempts, exact source/tool identities, full inventory and configured skips.
- AC4: Update reproducible operator guidance and redacted evidence, self-review, intentionally commit the slice, and reconcile command/host ownership and integration handoff.

## Boundaries

No release, signing, publication, version bump, Actions, or live remote mutation.
Published source/tag/assets and existing producer roots remain frozen.
Native evidence uses an isolated root in the approved interactive Windows context.
Coordinate exclusive physical-PC use with BPR-1653; do not touch unrelated
waiter PID 37820, global auth/signing state, or other agents' work.
RFC-BPR-0038 qualification remains open independently.
