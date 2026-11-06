---
schema: "implement-rfc.card.v1"
id: "BPR-1898"
title: "Separate Overview progress readiness from network status"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1897"]
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1898--BPR-1898--605c229d551e.json","milestone_plan":{"id":"responsiveness-latency-v10","path":"docs/rfcs/plans/responsiveness-latency-v10/milestone-plan.json"},"frontend_design":"frontend-design:frontend-design and desktop guidance; retain existing Status sections."}
---

# BPR-1898: Separate Overview progress readiness from network status

Continue the owner's direct responsiveness work after independent network
Status sections. Local receipts and private records have their own asynchronous
loads; an unfinished load must not look like zero completed work or keep ready
reviewer/merge/check sections in a loading state.

## Acceptance

- AC1: Represent local progress readiness separately from network Status;
  publish available progress as its scoped local inputs complete and explicitly
  identify still-loading or unavailable inputs rather than inventing zeroes.
- AC2: Preserve durable progress meanings, file/filter/account/PR identity,
  action authority, cancellation/retirement, and existing final values. Reuse
  the current asynchronous store ownership and avoid duplicate effects.
- AC3: A composed held-local-load course demonstrates usable network Status
  while progress loads, then correct progress completion without disturbing
  check selection or disclosures; failure and replacement remain explicit.
- AC4: Pass focused optimized functional/interaction and coding/architecture/
  documentation checks, quantify the improvement or independent readiness,
  add a journal line and land the verified round on main.

## Plan

Inspect the existing ledger completion boundaries and use narrow readiness and
presentation updates. Preserve the accepted authority required to open private
storage. Reuse the My review section and current loading presentation, applying
frontend-design desktop guidance; no new dependency or general-purpose system.
