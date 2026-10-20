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
separates 159.39 minutes of suite wall from 9.47 minutes of combined final
configure/build/bridge overhead; blanket serial rows consumed 147.78 minutes.
The Settings-parent experiment was rejected/reverted. Fresh-byte checker sharing
passes 159 native adversarial methods and saves 9.30 seconds in its matched
direct gate; that small change is not the systemic outcome.

A four-row same-binary native scheduling control passed at 443.07 seconds serial
versus 169.36 seconds concurrent (61.8% less wall, 7.1% more summed row time).
The 784-row property audit preserves commands, fixtures, assertions, budgets,
timeouts, containment and existing locks. Source audit narrows the proposed
cohort to 34 rows / 31.43 minutes of retained exclusive work; timed Markdown,
shared canaries, native/global-resource and unclassified courses stay exclusive.
All 34 expanded native rows pass in 648.88 seconds wall (1952.73 summed).
The remaining Debug build took 515.489 seconds, with 744 objects already present
and 879 created/updated; it is separate from suite timing. The isolated cold
composition row failed in 1302.97 seconds on a stale BPR-1669 source-manifest
review pin, then continued through its later cold build. The owning-source review
and reseal are recorded on BPR-1669; the failed run remains retained evidence.
Existing unittest fail-fast now prevents expensive downstream methods after a
failed tripwire, reporting failure and explicit not-run count. Its actual cheap
method is documented as early feedback before long gates. All eight original
method bodies remain; one comparable complete native gate remains pending.
The nested build now respects the existing producer compiler-job setting,
capped at eight with default two and invalid settings refused. It remains
exclusive and finishes compilation before runtime checks; no extra cold build
or measured cold-build saving is claimed. Nine maintained runner controls and
actual build-argument controls cover that setting.
Root workflow commit `89fde0df1` is preserved and its guidance reconciled.
No full-suite or end-to-end release saving is claimed yet.

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
The unchanged history control passed in 425.723 seconds, separate from its
170.949-second target build. The coordinator authorized the bounded scheduling
design after aggregate diagnosis; the causal pilot above justifies its locked
correctness cohort. Full acceptance remains open until the native gate passes.
