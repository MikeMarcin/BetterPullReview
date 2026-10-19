---
schema: "implement-rfc.card.v1"
id: "BPR-1632"
title: "Diagnose large-review history input gap"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"f6ba8f43e4477f60941dd24cdac4450ca68de911","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1632: Diagnose large-review history input gap

The frozen Mac baseline failed `application_large_review_history`. The coordinator
assigns read-only metric/source diagnosis while that full baseline continues.
No heavy Mac build or reproduction is authorized before its completion, and
no application repair is authorized before a source-backed cause is established.

## Acceptance

- AC1: Bind the actual report and failing attributed metric to the exact frozen source, preserving original platform budgets and timing semantics; distinguish observations from unproved cause.
- AC2: After coordinator dispatch permits Mac reproduction, establish the failing operation with focused native evidence and retain all original failures; no transient-load assumption or threshold relaxation.
- AC3: Implement only a justified bounded repair agreed with the coordinator, verify the exact candidate under unchanged meaningful limits, self-review and commit explicit paths, and return quiescent evidence. If diagnosis establishes no justified source repair, reconcile that evidence with the coordinator before closeout.

## Initial metric observation

The frozen report has one violating refresh, `epoch_refresh_4`: attributed GUI
pulse gap 142.3115 ms against 100 ms, while completion is 440.256 ms against
12,000 ms. The corresponding wall gap is 145.307833 ms, GUI processor time
1.433292 ms and observed runnable wait 2.996333 ms. Thus 140.878208 ms is
non-CPU time retained by attribution. This identifies the failing metric but
not its operation or cause; no stack was captured at that gap. The original
report digest and exact phase are preserved in ignored
`.mask/sessions/release-v0.3.0/history-initial-diagnosis.json`. No formula or
threshold changed. Native reproduction remains pending the coordinator's
baseline and prescribed release-fuzz completion.
