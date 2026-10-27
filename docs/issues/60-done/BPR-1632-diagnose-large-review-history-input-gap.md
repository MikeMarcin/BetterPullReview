---
schema: "implement-rfc.card.v1"
id: "BPR-1632"
title: "Diagnose large-review history input gap"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"f6ba8f43e4477f60941dd24cdac4450ca68de911","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"triage_20261026":{"date":"2026-10-26","disposition":"stale","reason":"The v0.3.0 history input-gap miss does not recur in Release; v0.3.4's macOS, Windows and Ubuntu Release suites pass the course."}}
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

## Bounded native follow-up

The unchanged frozen source passes its unprofiled focused rerun in 149.25
seconds. One authorized diagnostic course on clean `adb1e7bf6` also passes,
with maximum refresh input gap 67.224 ms against the unchanged 100 ms limit.
Only its owned synthetic child is sampled at 10 ms; the report is explicitly
perturbed diagnostic evidence, not unprofiled acceptance. Aggregate stacks show
GUI allocation waiting on malloc's native fork lock and a worker in fork. They
do not identify the original failed interval or establish its unique cause.
Original report, passing unprofiled report, sampled report, full stacks and
process receipts are retained under ignored `history-diagnosis/`. All three
owned course/sample processes finish. No timing source, budget, attribution or
retry policy changes; no further diagnostic course is authorized this round.
Coordinator disposition remains pending, so this card stays In Progress.

## Closed in triage, 2026-10-26

Closed as stale. The one 142 ms miss came from v0.3.0's frozen Mac
baseline. The unprofiled rerun on that same source passed, the coordinator
never dispatched a repair, and the release closed long ago.
`application_large_review_history` passes in the v0.3.4 Release suites on
macOS, Windows and Ubuntu. Debug misses are nongating under the owner's
[standing policy](../../development/release-test-profiles.md#standing-debug-performance-policy),
and Ubuntu omits that course in Debug. No cause was established, and the
retained malloc fork-lock stacks are diagnostic only. Open a new card if
the miss recurs in Release.
