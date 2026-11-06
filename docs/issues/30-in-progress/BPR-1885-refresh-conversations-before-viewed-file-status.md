---
schema: "implement-rfc.card.v1"
id: "BPR-1885"
title: "Refresh conversations before viewed-file status pagination"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1885--BPR-1885--cb56fb646bfe.json","related_cards":["BPR-1883"],"milestone_plan":{"id":"responsiveness-latency-v2","path":"docs/rfcs/plans/responsiveness-latency-v2/milestone-plan.json"}}
---

# BPR-1885: Refresh conversations before viewed-file status pagination

The owner requested continued direct responsiveness work on 2026-11-06,
especially independent loading, with every round landed on main, a card for
each change and a one-line journal entry. Round one landed at `661981be0`.

The GraphQL pager exhausts viewed-file status pages before completing review
threads. Those statuses do not decide conversation content, but they delay the
same-head refresh observation on pull requests with many changed files.

## Acceptance

- AC1: Eligible authenticated refreshes publish complete reviews and review
  threads before viewed-file pagination; keep first-open and unobserved ordering,
  exact final records and request counts.
- AC2: Exhaust and validate every viewed-file page before accepting a snapshot;
  retain cursor, count, identity, limit and cancellation checks, including a
  failure or cancellation after the conversation is shown.
- AC3: Verify the composed refresh still displays pending work and its comparison;
  record optimized before/after latency and a negative control.
- AC4: Pass focused source/documentation checks, record inherited failures
  accurately, complete the journal/card and land the round on main.

## Evidence

Pending implementation and verification.
