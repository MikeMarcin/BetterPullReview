---
schema: "implement-rfc.card.v1"
id: "BPR-1991"
title: "Give the Tour tab a head, a scrolling route and a pinned agent bar"
status: "In Progress"
rfcs: ["RFC-BPR-0099"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0003-tour-tab-bands.md"}
depends_on: []
---

# Give the Tour tab a head, a scrolling route and a pinned agent bar

## Scope and acceptance

Implements
[RFC-BPR-0099](../../rfcs/RFC-BPR-0099-tour-publishing-in-reach-and-reported.md).

Rebuild the Tour tab as three bands with only the route scrolling, add the
head with its state table and single publication action, keep the agent bar at
the bottom at every route length, and remove the bottom Preview and publish
button.

Covers AC01 and AC02. The head's Publishing and Stopped states need the per-
step projection from BPR-1992 and are completed there. Load frontend-design
before the Qt work.

This is part of `tour-publishing` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
