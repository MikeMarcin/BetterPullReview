---
schema: "implement-rfc.card.v1"
id: "DRAFT-0003"
title: "Give the Tour tab a head, a scrolling route and a pinned agent bar"
status: "Backlog"
rfcs: ["RFC-DRAFT-0002"]
depends_on: []
---

# Give the Tour tab a head, a scrolling route and a pinned agent bar

## Scope and acceptance

Implements
[RFC-DRAFT-0002](../RFC-DRAFT-0002-tour-publishing-in-reach-and-reported.md).

Rebuild the Tour tab as three bands with only the route scrolling, add the
head with its state table and single publication action, keep the agent bar at
the bottom at every route length, and remove the bottom Preview and publish
button.

Covers AC01 and AC02. The head's Publishing and Stopped states need the per-
step projection from DRAFT-0004 and are completed there. Load frontend-design
before the Qt work.

This is part of `tour-publishing` in the [plan](../milestone-plan.json).
