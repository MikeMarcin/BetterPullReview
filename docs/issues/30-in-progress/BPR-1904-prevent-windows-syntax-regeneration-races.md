---
schema: "implement-rfc.card.v1"
id: "BPR-1904"
title: "Prevent Windows syntax dependency regeneration races"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"discovered_by":"BPR-1901","milestone_plan":{"id":"windows-syntax-regeneration-v1","path":"docs/rfcs/plans/windows-syntax-regeneration-v1/milestone-plan.json"}}
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
