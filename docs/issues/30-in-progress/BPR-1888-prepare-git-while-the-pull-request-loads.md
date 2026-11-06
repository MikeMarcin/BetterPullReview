---
schema: "implement-rfc.card.v1"
id: "BPR-1888"
title: "Prepare Git while the pull request loads"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1888--BPR-1888--c401c5ea9d04.json","related_cards":["BPR-1842"],"milestone_plan":{"id":"responsiveness-latency-v5","path":"docs/rfcs/plans/responsiveness-latency-v5/milestone-plan.json"}}
---

# BPR-1888: Prepare Git while the pull request loads

The owner requested continued direct responsiveness work, especially independent
loads with clear pending states, and each round on main with a card and journal
line. BPR-1842 identified the first installed-Git admission as work still delayed
until snapshot acceptance. Its executable/version/capability checks are independent
of repository identity and can run while the GitHub read is in flight.

## Acceptance

- AC1: Start optional, bounded Git-runtime preparation with an admitted PR read;
  perform no repository/cache-session work and grant no snapshot authority early.
- AC2: Reuse the existing exact executable/isolation-root admission checks;
  serialize overlapping preparations, retain cancellation and foreground fallback,
  and keep preparation failures from changing the read's result.
- AC3: Account for preparation in host shutdown/quiescence, avoid repeated jobs
  and preserve constrained foreground admission; verify real adapter and composed
  opening paths, including clear loading states and unchanged read counts.
- AC4: Record optimized before/after latency, focused checks and inherited
  failures; complete the card and one-line journal entry and land on main.

## Evidence

Pending implementation and verification. This overlaps setup only; the existing
accepted snapshot, epoch handoff and file-loading indicators remain authoritative.
