---
schema: "implement-rfc.card.v1"
id: "BPR-1670"
title: "Speed up the native Windows suite without weakening coverage"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"kind":"corrective","origin":"Owner mandate 2026-10-20: Have an implementer look into why the Windows suite takes almost 3 hours and speed it up without weakening the tests.","reservation":"docs/issues/_reservations/BPR-1670--BPR-1670--1dbc02b123e1.json","milestone_plan":{"id":"bpr1670-windows-suite-speedup","path":"docs/rfcs/plans/bpr1670-windows-suite-speedup/milestone-plan.json"}}
---

# Speed up the native Windows suite without weakening coverage

## Mandate and baseline

Original mandate revision `windows-suite-speedup-r1`, source baseline `3547e8ef6`.
The retained exact v0.3.1 native Windows pass at `2ed7e5f98641d75678f861dad0edbe658b5f5d22`
completed 784 rows (783 pass, one configured Debug skip), four jobs, in
9563.163747 seconds wall time and 11649.0845232 summed test seconds.
408 serial rows account for 8866.9004852 seconds. Compare matched focused
controls before spending a further full gate; distinguish cold/warm setup.

## Acceptance

- AC1: Explain native Windows build versus suite wall/work time, serial classification and repeated setup, together with the preserved release critical path, using retained receipts and focused causal controls.
- AC2: Demonstrate a meaningful native runtime improvement through a behavior-preserving engineering correction, retaining every maintained test, fixture/workload, assertion, budget, timeout, containment boundary, network isolation rule, resource lock and four-job policy; keep performance-sensitive and global-resource courses exclusive, and allow concurrency only where source and causal native controls prove equivalent isolation and acceptance.
- AC3: Run focused positive/adversarial regressions against the exact candidate and one complete comparable native Windows suite, preserving raw failed attempts, exact source/tool identities, full inventory and configured skips.
- AC4: Update reproducible operator guidance and redacted evidence, self-review, intentionally commit the slice, and reconcile command/host ownership and integration handoff.

## Boundaries

No release, signing, publication, version bump, Actions, or live remote mutation.
Published source/tag/assets and existing producer roots remain frozen.
Native evidence uses an isolated root in the approved interactive Windows context.
Coordinate exclusive physical-PC use with BPR-1653; do not touch unrelated
waiter PID 37820, global auth/signing state, or other agents' work.
RFC-BPR-0038 qualification remains open independently.

## Current engineering checkpoint

The [runtime investigation](../../verification/BPR-1670-windows-suite-runtime.md)
records the retained full pass, causal controls and rejected attempts. The
Settings-parent experiment passed its public course and 53 identical PNGs,
but did not improve runtime and was reverted in `28887de99`. The checker-only
candidate shares fresh source bytes inside one invocation; its direct native
gate improved from 41.0335 to 31.7367 seconds. Native adversarial and full-suite
acceptance remain open; the isolated adversarial row is being measured before
spending another full gate. No test, workload or limit change is accepted or proposed.

External command/evidence ownership lives at
`/tmp/bpr-windows-suite-speedup/handoff.md`. The native isolated root is
`C:/Users/mikem/bpr1670-suite`; the exclusive shared-PC lease is reconciled with
BPR-1653 at every handback. No human criterion is parked.

## Owner amendment: systemic cause first

Mandate revision `windows-suite-speedup-r2` supersedes the original scheduling
assumption. The owner said the task felt focused on microoptimizations when
there was a systemic problem in the three-hour build and long release. Before
further implementation, diagnose blanket serialization, repeated startup and
configuration/build work, cold/warm conditions, obsolete candidate reruns and
release orchestration. Preserve coverage, workload, diagnostics, thresholds,
security and isolation. The owner permits correcting overbroad scheduling with
causal proof; performance-sensitive rows still need an exclusive host.

The uncommitted path experiment was discarded without a native candidate build.
The already-running unchanged history control is retained and may finish; no
full gate is justified by the checker saving alone. A bounded design with
aggregate affected work and isolation controls must precede implementation.
