---
schema: "implement-rfc.card.v1"
id: "BPR-2086"
title: "Wait for source geometry before measuring Find scroll work"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"milestone_plan":{"id":"bpr-2086-source-find-ready-baseline","path":"docs/rfcs/plans/bpr-2086-source-find-ready-baseline/milestone-plan.json"},"mandate_revision":"release56-source-find-ready-r1","source_baseline":"48bd9b3f2b82128e6b43527812f165eacc4f8b47","allocation_main":"9323372ba","reservation":"docs/issues/_reservations/BPR-2086--BPR-2086--1c67e2892878.json"}
---

# Wait for source geometry before measuring Find scroll work

All three normal56 Windows Debug scales fail the same four-counter invariant.
The once-only 200% scalar diagnostic records geometry pending before the baseline
and only pane measurements changing 50 to 52, with geometry/index/matching/history
unchanged after hiding line numbers. The fixture must finish pre-existing work
before measuring work caused by its scroll/navigation gestures.

## Acceptance

- AC1. Retain original failures and scalar evidence. After the original 180ms
  settle and before the rectangle/statistics baseline, wait through the existing
  bounded helper for public `!geometry_pending()`. Keep every original gesture,
  workload, rectangle assertion and all four exact counter equalities.
- AC2. A private negative control delivers a real font-change geometry event
  after the baseline and is rejected by the unchanged counter oracle. Corrected
  normal Release and Debug courses at all three scales use actual normal flags,
  frozen product inputs, isolation, properties and original budgets. Keep actual
  Debug cutoffs explicit under standing policy, with no functional pass credit.
  Format22, tidy and scoped documentation/board checks pass.
- AC3. Land only owned test/workflow commits on main, preserving foreign work and
  counter highwater. Frozen56 producers remain unchanged; all owned processes and
  writers are accounted for and leases returned. Engineering proof does not replace
  fresh common-source qualification.
