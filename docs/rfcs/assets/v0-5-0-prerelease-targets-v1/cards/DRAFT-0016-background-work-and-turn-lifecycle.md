---
schema: "implement-rfc.card.v1"
id: "DRAFT-0016"
title: "Establish how agents report background work and end a turn when the work ends"
status: "Backlog"
rfcs: ["RFC-DRAFT-0006"]
depends_on: []
---

# Establish how agents report background work and end a turn when the work ends

## Scope and acceptance

Implements
[RFC-DRAFT-0006](../RFC-DRAFT-0006-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md).

First record, for each installed agent and transport, how sub-agents and
background tasks are reported and whether the agent can be run with helpers
visible or must run foreground only. Then decode the runtime's activity and
helper events, keep a turn open while reported helpers are outstanding, accept
that turn's later frames, end every turn as an answer, Stopped or a stated
error, and extend the run diagnosis as the RFC states.

Covers AC01 to AC04 and AC07. This amends RFC-BPR-0096's late-frame rule only
for frames that continue an open turn; a frame for no open turn is still
refused.

This is part of `agent-work-visible` in the [plan](../milestone-plan.json).
