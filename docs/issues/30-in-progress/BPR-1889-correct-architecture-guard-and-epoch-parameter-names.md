---
schema: "implement-rfc.card.v1"
id: "BPR-1889"
title: "Correct architecture guard and epoch parameter names"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1888"]
priority: "P1"
metadata: {"kind":"maintenance","reservation":"docs/issues/_reservations/BPR-1889--BPR-1889--426a20fdc16c.json","milestone_plan":{"id":"responsiveness-latency-v6","path":"docs/rfcs/plans/responsiveness-latency-v6/milestone-plan.json"}}
---

# BPR-1889: Correct architecture guard and epoch parameter names

After BPR-1888's latency verification reported the inherited failures, the owner
asked what the architecture error and three naming warnings were and requested
that they be fixed next. This round preserves runtime behavior and restores the
relevant checks rather than carrying these known failures forward.

## Acceptance

- AC1: Distinguish managed-clone Git-directory paths from misplaced neutral
  working-directory validation; the real architecture gate passes and focused
  positive/hostile controls preserve its ownership restriction.
- AC2: Update the architecture contract's stale synthetic runner fixture to
  include the existing managed-clone factory/deadline policy, keeping the
  production guard and its failure controls meaningful; the contract suite passes.
- AC3: Rename the three nonconforming `materialize_on_actor` parameters without
  changing behavior; the full epoch-host clang-tidy 22 check and focused host
  functional course pass.
- AC4: Record checks and any remaining inherited gaps, add a one-line journal
  entry, complete the card and land the round on main.

## Evidence

Pending implementation and verification. The existing BPR-1879/BPR-1884 clone
policy, process ownership, credentials, limits and runtime behavior stay intact.
