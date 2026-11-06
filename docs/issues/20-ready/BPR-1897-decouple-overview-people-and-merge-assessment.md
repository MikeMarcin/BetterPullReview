---
schema: "implement-rfc.card.v1"
id: "BPR-1897"
title: "Decouple Overview people and merge assessment"
status: "Ready"
rfcs: []
depends_on: ["BPR-1895"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1897--BPR-1897--d081652bf43f.json","milestone_plan":{"id":"responsiveness-latency-v10","path":"docs/rfcs/plans/responsiveness-latency-v10/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance; retain existing Status sections."}
---

# BPR-1897: Decouple Overview people and merge assessment

The owner requests independent Overview reviewers, merge rules and progress,
working directly, preserving other behavior and landing each verified round on
main with a card and one journal line.

Merge assessment arrives before reviewer requests, and the complete reviewer
roster arrives before branch rules/protection. Present those read-only sections
at those boundaries without reordering or adding requests. Keep rules and local
progress explicitly loading until their own data is ready.

## Acceptance

- AC1: Observed authenticated first opens publish merge assessment and the
  complete people/request roster independently, before remaining policy reads;
  checks retain their earlier publication. Refresh/signed-out/unobserved paths,
  request ordering/counts, shared budgets and final validation stay unchanged.
- AC2: Admit each bounded observation once after metadata for the exact current
  read; validate records, identities, shape and text bounds and refuse invalid,
  stale, mixed, cancelled or terminal observations. Grant no accepted snapshot,
  epoch, mutation or link authority.
- AC3: In the composed offscreen app, held reviewer/rule reads leave completed
  Status sections readable with clear remaining loading. Disclosure and check
  selection survive later sections and final acceptance; replacement/failure
  clears provisional content. Old-reader controls fail the early-content claim.
- AC4: Measure optimized before/after section visibility; pass focused provider,
  core/host, Qt/public-path, architecture, coding and documentation checks,
  record one journal line and land on main.

## Plan

Reuse the existing observation channel, bounded core admission, shared result
policies and Status sections. Publish compact participant facts without copying
comment bodies. Keep unavailable data distinct from still-loading data. Apply
frontend-design guidance to the existing desktop surface, with no redesign.
