---
schema: "implement-rfc.card.v1"
id: "DRAFT-0017"
title: "Show what a running turn is doing, its helpers and when it goes quiet"
status: "Backlog"
rfcs: ["RFC-DRAFT-0006"]
depends_on: ["DRAFT-0016"]
---

# Show what a running turn is doing, its helpers and when it goes quiet

## Scope and acceptance

Implements
[RFC-DRAFT-0006](../RFC-DRAFT-0006-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md).

Present the running block (what the agent is doing, elapsed time, time since
the last frame, helper rows, Stop), fold it under the answer after the turn,
and show the 120-second quiet notice with its two choices, in the Agent tab
and the tour side chat.

Covers AC05, AC06, AC08 and the owner's look in AC09. Load frontend-design
before the Qt work.

This is part of `agent-work-visible` in the [plan](../milestone-plan.json).
