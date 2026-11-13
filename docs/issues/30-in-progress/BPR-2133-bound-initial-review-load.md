---
schema: "implement-rfc.card.v1"
id: "BPR-2133"
title: "Bound the common initial review-load GUI latency"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "win", "mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2133--BPR-2134--55002d8d830d.json","mandate_revision":"linux-release-blockers-r1","source_baseline":"6efba00e1afbf874c88c7e5de2623a33528ff36f","milestone_plan":{"id":"linux-release-blockers-r1","path":"docs/rfcs/plans/linux-release-blockers-r1/milestone-plan.json"}}
acceptance: []
---

# Bound the common initial review-load GUI latency

Six frozen60 Linux Release large-review courses exceed the unchanged100ms input target only in paged_review_and_git_load:113..188ms. Actual CPU74..101ms and nonCPU13..98ms do not establish the responsible callback or lock. All other measured phases and shutdown pass. Diagnose and correct the actual common operation, without timing waivers or retries-to-green.

The owner explicitly directed fixing the Linux blockers on Main, then taking
only those fixes into v0.4.2. Frozen R/W source and original native courses stay
immutable. No broad Main intake, unrelated refactor or private product deviation.

## Acceptance

- AC1: Bind original failure and establish an operation-level cause with bounded native evidence, preserving original source/profile/oracles and distinguishing observations from hypotheses.
- AC2: Implement the smallest justified correction and meaningful regression control without loosening Release100ms input gates, the submission20s functional predicate, or unrelated assertions. Record exact affected source/dependency/input closure and native regression selection; no unproved narrowness.
- AC3: Run meaningful focused native verification on the exact private candidate under original configured context/containment/limits. Keep actual failures and all protected producer seals; no qualification or unchanged-platform credit without an explicit bridge reviewed by the coordinator.
- AC4: Land only owned source/test/issue/plan/proof commits on current Main, preserving unrelated work/schema drift; return explicit commit mapping and terminal host/command state for selective intake. Source/legal/native trust/signing/publication and final release acceptance remain the coordinator's obligations.

## Owner release amendment, 2026-11-12

The owner subsequently said: “Are the only remaining blockers latency? If so
waive them.” The release coordinator records this as conditional acceptance of
only the six observed optimized Linux initial-load input misses after functional
blockers clear. Performance diagnosis/builds are held; this issue is not fixed
or Done. Original failures and their actual/unreached checks remain evidence.
An instrumented history diagnostic was below the limit and is a nonreproduction,
not qualification. No product performance correction has been made or proposed
for release on that basis.
