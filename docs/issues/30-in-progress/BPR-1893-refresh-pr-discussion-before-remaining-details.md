---
schema: "implement-rfc.card.v1"
id: "BPR-1893"
title: "Refresh PR discussion before remaining details"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1890"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1893--BPR-1893--901ac9044cbf.json","milestone_plan":{"id":"responsiveness-latency-v8","path":"docs/rfcs/plans/responsiveness-latency-v8/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance applied; reuse existing discussion layout and remaining-loading plate."}
---

# BPR-1893: Refresh PR discussion before remaining details

The owner requested continued direct responsiveness work, especially decoupled
loads with clear loading states, preserving other behavior and landing each
round on main with a card and one-line journal entry.

Inline replies now appear before independent file and detail reads, but PR-wide
comments remain invisible until the whole refresh completes. Give complete
PR-wide comments their own bounded observation after the inline conversation,
before the catalogue, without delaying the existing early inline reply.

## Acceptance

- AC1: An observed authenticated refresh publishes complete PR-wide comments
  before file pagination and remaining details. Inline conversation remains
  earlier; first-open, signed-out and unobserved reads keep their order. Final
  validation, record/request counts, budgets, failure and cancellation remain.
- AC2: Core and host admit each independently observed conversation collection
  at most once after metadata, only for the exact current refresh of the
  accepted PR, head, base and mode. Early content adds no snapshot or epoch
  authority, and stale, duplicate, invalid or terminal observations are refused.
- AC3: In the composed offscreen application, updated PR-wide comment text is
  readable while the catalogue is held, with remaining loading clearly shown;
  the comparison and reading position survive. The old-order control fails.
- AC4: Measure optimized before/after visibility, pass focused functional,
  architecture and coding checks, record one journal line and land on main.

## Plan

Use the existing observation channel and serialized lane. Add one independent
discussion observation containing complete issue comments and their worker-side
body digests. Share exact refresh admission with inline conversations, while
tracking their one-time acceptance separately. Reuse the existing conversation
projection and partial loading plate. Exercise both orders, invalid/stale data,
shared limits and held-catalogue interaction through the real composition.
