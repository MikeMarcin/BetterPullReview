---
schema: "implement-rfc.card.v1"
id: "BPR-1895"
title: "Show Overview checks before remaining status"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1893"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1895--BPR-1895--a728bb8ad4fb.json","milestone_plan":{"id":"responsiveness-latency-v9","path":"docs/rfcs/plans/responsiveness-latency-v9/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance applied; retain the existing Checks section and partial loading plate."}
---

# BPR-1895: Show Overview checks before remaining status

The owner asked for Overview's Status tab to load faster when opening a PR,
allowing fast sections to appear before slow sections with explicit loading.
Continue direct implementation, preserve other behavior, record one journal
line and land each verified round on main. Work resumes from rewritten history.

The provider already has complete check runs and commit statuses before it reads
merge assessment, pending reviewers and branch policy. Overview currently waits
for the entire snapshot before showing any of those results. Publish the check
results independently, without changing request order or spending extra requests.

## Acceptance

- AC1: An observed authenticated first open publishes complete check results
  before remaining merge/reviewer details. Refresh, signed-out and unobserved
  reads retain their behavior; request counts, ordering, shared bounds,
  optional-failure handling and final snapshot validation are preserved.
- AC2: Core/host accept one bounded check observation after metadata only for
  the exact pending read. Check shape, head, identities and text/record bounds
  are validated; duplicate, stale, cancelled and terminal observations are
  refused. This publishes no accepted snapshot, epoch or mutation authority.
- AC3: In the composed offscreen app, Status shows selectable check results
  while merge assessment is held, with remaining status clearly loading.
  Check details stay readable, final acceptance retains selection/disclosure,
  and superseded or failed opens clear provisional results. The old-reader
  control fails the early-results assertion.
- AC4: Measure optimized before/after latency with equal request counts; pass
  focused core/host/provider/Qt/public-path and architecture/coding/documentation
  checks, add one journal line and land the verified card on main.

## Plan

Reuse the overview observation channel with one independent check collection;
project it with the existing check-result policy and no merge-policy claims.
Reuse the current Checks section with a partial loading plate for the remaining
Status data. Browser actions keep waiting for the accepted review authority.
Exercise the provider, exact host lifecycle, widget state and actual window
interaction. No layout redesign, new worker or transport concurrency is needed.
