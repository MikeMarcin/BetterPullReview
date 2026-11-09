---
schema: "implement-rfc.card.v1"
id: "BPR-1958"
title: "Wait for the scaling course's composed Review readiness"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"test","found_by":"Rejected v0.4.2 build43 native Windows qualification","mandate":"Owner release-blocker fixes land through an implementer on main; coordinator scoped 2026-11-09 readiness correction to application_scaling_tests.cpp only.","milestone_plan":{"id":"bpr-1958-scaling-readiness","path":"docs/rfcs/plans/bpr-1958-scaling-readiness/milestone-plan.json"}}
---

# BPR-1958: Wait for scaling Review readiness

## Outcome and acceptance

- AC1. The scaling public course enters Review through its existing shared
  modality helper and waits for the exact file's current projection before
  selecting it. Workloads, measured gestures, allocation and semantic oracles,
  network boundary, timing policy and original watchdogs remain intact.
- AC2. Retain the original Windows failures and distinguish a meaningful
  negative control from corrected composed evidence. Bind native evidence to
  exact source, image, arguments and context; no final release acceptance is
  supplied by this engineering course.
- AC3. Focused verification, formatting and documentation checks pass, and
  the correction lands on main. Preserve all unrelated rejected-build failures.

## Scope

Only `tests/end_to_end/application_scaling_tests.cpp` changes product-test
behavior. The rejected frozen43 producer and its images remain immutable.
The eight Debug rows that could not start have a separate damaged-image
cause; startup, other Review readiness, notification and optimized timing
failures remain separate qualification obligations.

The Windows course immediately looked up `src/file_0031.cpp` after triggering
Review, before delivering the host's pending projection. Build43 Release
originally stopped during startup; Debug stopped at `actual file row exists`.
A serial engineering repetition reached that same file-row assertion in
Release. Its Release portion overlapped an accidentally surviving diagnostic
receipt query and supplies no quiet-machine timing evidence. The diagnostic
process was stopped by exact owned PID, creation and script identity.
