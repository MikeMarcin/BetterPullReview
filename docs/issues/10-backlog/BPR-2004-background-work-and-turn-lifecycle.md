---
schema: "implement-rfc.card.v1"
id: "BPR-2004"
title: "Establish how agents report background work and end a turn when the work ends"
status: "Backlog"
rfcs: ["RFC-BPR-0103"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0016-background-work-and-turn-lifecycle.md"}
depends_on: []
---

# Establish how agents report background work and end a turn when the work ends

## Scope and acceptance

Implements
[RFC-BPR-0103](../../rfcs/RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md).

First record, for each installed agent and transport, how sub-agents and
background tasks are reported and whether the agent can be run with helpers
visible or must run foreground only. Then decode the runtime's activity and
helper events, keep a turn open while reported helpers are outstanding, accept
that turn's later frames, end every turn as an answer, Stopped or a stated
error, and extend the run diagnosis as the RFC states.

Covers AC01 to AC04 and AC07. This amends RFC-BPR-0096's late-frame rule only
for frames that continue an open turn; a frame for no open turn is still
refused.

This is part of `agent-work-visible` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
