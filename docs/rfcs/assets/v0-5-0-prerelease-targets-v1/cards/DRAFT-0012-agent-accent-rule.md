---
schema: "implement-rfc.card.v1"
id: "DRAFT-0012"
title: "Apply the agent accent to exactly the controls that start a run"
status: "Backlog"
rfcs: ["RFC-DRAFT-0004"]
depends_on: []
---

# Apply the agent accent to exactly the controls that start a run

## Scope and acceptance

Implements [RFC-DRAFT-0004](../RFC-DRAFT-0004-agent-chat-conventions.md).

Add the agent variant and its test to docs/ux/shared-action-buttons.md, give
the accent to every control that dispatches an agent run and remove it from
every control that does not, across the Agent tab, the side chats, the tour,
the comment workshop and Review Cycles, and produce the inventory that shows
the two sets are equal.

Make the accent a palette role, Agent accent, with per-variant built-in
values and a row on Appearance › Colours under the existing rules of
RFC-BPR-0034, and have every accented surface read the role instead of a
fixed value.

Covers AC07, AC09 and the owner's look in AC08. Load frontend-design before the Qt
work.

This is part of `agent-tab-asks` in the [plan](../milestone-plan.json).
