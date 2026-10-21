---
schema: "implement-rfc.card.v1"
id: "BPR-1670"
title: "Speed up the native Windows suite without weakening coverage"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"kind":"corrective","origin":"Owner mandate 2026-10-20: Have an implementer look into why the Windows suite takes almost 3 hours and speed it up without weakening the tests.","reservation":"docs/issues/_reservations/BPR-1670--BPR-1670--1dbc02b123e1.json","milestone_plan":{"id":"bpr1670-windows-suite-speedup","path":"docs/rfcs/plans/bpr1670-windows-suite-speedup/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"6757e91b0c4d59eb2ac42e8509071c4f3d525e05","evidence":{"kind":"artifact","detail":"docs/verification/BPR-1670-windows-suite-runtime.md: retained native wall/work/serial/build/release decomposition; 408 serial rows 147.78min, actual cold1132.09s compiler span and failed1302.97s course retained; clean bootstrap not claimed."}},{"criterion":"AC2","outcome":"pass","revision":"6757e91b0c4d59eb2ac42e8509071c4f3d525e05","evidence":{"kind":"artifact","detail":"Matched same-binary native4 controls PASS443.07s serial versus169.36s concurrent; expanded34 PASS648.88s; full784/property/472binary guards preserve every substantive acceptance and existing lock. Full observed136.02 versus159.39min is conditional14.66%, not30min release."}},{"criterion":"AC3","outcome":"pass","revision":"6757e91b0c4d59eb2ac42e8509071c4f3d525e05","evidence":{"kind":"artifact","detail":"Full d989 all784 exercised780PASS3missing-toolingFAIL1unchangedDebugSKIP/CTest8; rawtar f71c5896e7727eeb742063c7c7fdbf05c35c2f6222d3e4da0a8311c3c0b6fb6e. Exactpinbootstrap+focused6757 actual3 and12runnercontrols PASS/full784manifest+472binaryidentities same; recoverytar12fdd8f1a92f4442eefb85968ca07c4b3feacffd7963fa3733f5c2ebf5c12562. Root accepted explicit combined evidence, no rawfull0 claim."}},{"criterion":"AC4","outcome":"pass","revision":"6757e91b0c4d59eb2ac42e8509071c4f3d525e05","evidence":{"kind":"artifact","detail":"Operator guards/guidance and redactedreport updated; selfreview corrected coarseworkbenchselector claim; local12runner+67documentation+2allocation and documentationgate PASS; native16exacttasks removed/ownprocessaudit0/PClease returned. Root89fde preserved; no production/fixture change or human obligation; unrelatedMASKdirtyfiles preserved."}}]
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

## Final engineering result

The [runtime investigation](../../verification/BPR-1670-windows-suite-runtime.md)
records the complete diagnosis, source/identity audit, failed attempts and controls.
A matched same-binary four-row native control passed at 443.07 seconds serial
versus 169.36 seconds concurrent (61.8% less wall). The audited Windows cohort
contains 34 rows; all original commands, assertions, fixtures, budgets, timeouts,
containment and existing locks remain. All 34 pass in 648.88 seconds standalone.
The rejected Qt parent experiment is reverted; no production/fixture change remains.

One complete native course at `d989efb5cebabbb180bed7faaf8fecee66d66ca5`
exercised all 784 rows in 8161.116 seconds / 136.02 minutes, with no missing or
extra rows. Raw result is 780 pass, three missing-tooling failures, one unchanged
configured Debug skip, CTest exit 8. It remains failed evidence. The observed
23.37-minute / 14.66% difference from the retained release is not an exact
same-source causal estimate or an end-to-end release saving. All eight composition
methods and both prior real release regression courses passed unchanged.

The missing ignored test dependency was restored through ordinary native MASK
at the committed pins. At readiness candidate
`6757e91b0c4d59eb2ac42e8509071c4f3d525e05`, the actual three affected checks pass
as preflight, then all four focused rows pass, including all 12 maintained runner
controls. The default 784-row graph and all 472 maintained binary identities match
across the full run and recovery. The exact source bridge changes only two Python
files and two operator documents; no runtime, fixture or scheduling change occurs.
The coordinator explicitly accepted this combined qualification boundary while
retaining the raw full failure; a second full run is not claimed.

Early feedback now uses the actual source-manifest and portable tooling checks.
A failed composition tripwire stops the remaining methods with explicit not-run
inventory and nonzero exit; accepted candidates retain all eight methods.
Nested compilation respects the producer's existing job setting, capped at eight,
default two, invalid settings refused. No cold speedup is claimed. The observed
515.489-second Debug build had 744 objects already present; it is not a clean
build measurement. The actual 1302.97-second cold composition failure is retained.

Root workflow `89fde0df1` is preserved and reconciled. Self-review and local
documentation/allocation controls pass. All native commands/collectors finished,
16 exact task-owned scheduled entries were removed, and the shared-PC lease was
returned. Native source/build evidence remains retained. Unrelated local MASK
updates are preserved outside this slice. Main integration belongs to root.
No human criterion is pending; RFC-BPR-0038 remains open independently.
External detailed ownership/evidence: `/tmp/bpr-windows-suite-speedup/handoff.md`.

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
correctness cohort. The final combined qualification above closes this bounded engineering scope;
the raw failed invocation is never relabeled as a passing full run.

## Owner amendments: release target and portfolio design

The owner subsequently set a 30-minute target from release request through
verified publication and requested an Astra Max read-only release architecture
and test-portfolio peer. This user-authorized design exception does not add an
engineering implementer or change this milestone's acceptance. The current
136.02-minute Windows course does not meet the new end-to-end target. No proposed
coverage reduction, qualification reuse or new host policy is adopted here.
The authoritative design and hashes are linked from the runtime investigation;
root owns that future release-wide work. The peer corrected an earlier coarse
workbench audit: the 100 ms drawer assertion and fixed ribbon output belong to
other selectors, and no collision among the three actual default scale commands
has been established. Their exclusion here is conservative, pending an exact
argv/reachable-resource audit. The current candidate remains unchanged.
