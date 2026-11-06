---
schema: "implement-rfc.card.v1"
id: "BPR-1882"
title: "Project review comment parents without repeated ancestry walks"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1882--BPR-1883--c940b7c58a89.json","milestone_plan":{"id":"responsiveness-latency-v1","path":"docs/rfcs/plans/responsiveness-latency-v1/milestone-plan.json"}}
---

# BPR-1882: Project review comment parents without repeated ancestry walks

The owner requested simplification and optimization focused on responsiveness
and latency on 2026-11-06, preserving other behavior and recording each notable
improvement in a one-line journal. Work directly, without delegated agents.

`project_github_pull_request` runs on the serialized decision lane. Every reply
currently walks its ancestors again to detect a cycle, making a deep thread
quadratic even though its parent relationships are immutable for the projection.

## Acceptance

- AC1: Resolve ancestry once per thread; preserve comment order, immediate parent
  identities, missing-parent fallback, and cycle/descendant fallback.
- AC2: Exercise reordered, branched, missing and cyclic relationships, plus
  large threads and the existing application-host projection path.
- AC3: Record before/after optimized measurements, focused checks and a one-line
  journal entry; preserve the existing authority and projection boundary.

## Evidence

Pending implementation and verification.
