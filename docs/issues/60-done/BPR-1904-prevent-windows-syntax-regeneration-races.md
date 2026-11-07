---
schema: "implement-rfc.card.v1"
id: "BPR-1904"
title: "Prevent Windows syntax dependency regeneration races"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"discovered_by":"BPR-1901","milestone_plan":{"id":"windows-syntax-regeneration-v1","path":"docs/rfcs/plans/windows-syntax-regeneration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"39ee4ea4a45387515d72103799228839644cf628","evidence":{"kind":"observation","detail":"Retained fresh native failure at bb70bb05e; corrected fresh parent configure builds both configurations successfully in 258.686s.","artifact":"docs/verification/windows-build-latency.md"}},{"criterion":"AC2","outcome":"pass","revision":"39ee4ea4a45387515d72103799228839644cf628","evidence":{"kind":"observation","detail":"Unchanged inputs recompile zero dependency objects; changed forwarded global rebuilds 83; root ZERO_CHECK remains and four native courses pass in each configuration.","artifact":"docs/verification/windows-build-latency.md"}},{"criterion":"AC3","outcome":"pass","revision":"39ee4ea4a45387515d72103799228839644cf628","evidence":{"kind":"observation","detail":"Source, completed card and journal ready for authorized main integration; evidence explicitly records unchanged warm timings and avoided failure/retry scope.","artifact":"docs/verification/windows-build-latency.md"}}]
---

# Prevent Windows syntax dependency regeneration races

Fresh native verification for [[BPR-1903]] exposed parallel Visual Studio
projects racing to restore the same CMake generation stamp. Concurrent
reconfiguration fails, and the parent's next attempt discards the incomplete
dependency build. Suppress nested regeneration for the parent-owned, pinned
source while preserving the parent's existing source/option invalidation.

- AC1: Preserve the observed native failure and complete fresh Debug and
  Release dependency builds with the correction.
- AC2: Unchanged inputs reuse compiled dependencies; changed forwarded inputs
  rebuild them. Root regeneration and required functional checks stay intact.
- AC3: Land the correction, completed card and one-line journal on main,
  distinguishing avoided failed retries from ordinary build-time improvement.

## Evidence

The [native investigation](../../verification/windows-build-latency.md) preserves
the failing fresh build and passing source `39ee4ea4a`: both configurations build,
unchanged inputs recompile zero dependency objects, a changed forwarded input
recompiles 83, root regeneration remains enabled and functional courses pass.
Paired warm retries show no ordinary speedup; this prevents failed-build retries.
