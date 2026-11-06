---
schema: "implement-rfc.card.v1"
id: "BPR-1883"
title: "Refresh conversations before waiting for commit pagination"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1882--BPR-1883--c940b7c58a89.json","related_cards":["BPR-1851"],"milestone_plan":{"id":"responsiveness-latency-v1","path":"docs/rfcs/plans/responsiveness-latency-v1/milestone-plan.json"}}
---

# BPR-1883: Refresh conversations before waiting for commit pagination

The owner explicitly permits decoupling loading on 2026-11-06 so useful content
can finish earlier, provided unfinished work remains clearly marked as loading.

BPR-1851 already publishes a refresh's complete reviews and review threads
before later collections, retaining the accepted snapshot's authority and the
existing partial-refresh indicator. Commit pagination still delays that
observation even though its contents do not depend on commits.

## Acceptance

- AC1: An authenticated refresh with observable review threads reads and
  publishes its complete conversation before commit pagination, without adding
  requests or changing first-open/public-preview read order.
- AC2: Final snapshot validation, cancellation, failures, identity checks and
  same-head-only early publication retain their contracts.
- AC3: The composed refresh displays the conversation earlier, keeps the
  comparison visible and marks remaining loading until the refresh completes.
- AC4: Record focused verification, optimized before/after measurements and
  a one-line journal entry.

## Evidence

Pending implementation and verification.
