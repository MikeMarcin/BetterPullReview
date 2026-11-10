---
schema: "implement-rfc.card.v1"
id: "BPR-2000"
title: "Apply the agent accent to exactly the controls that start a run"
status: "Ready"
rfcs: ["RFC-BPR-0101"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0012-agent-accent-rule.md"}
depends_on: []
---

# Apply the agent accent to exactly the controls that start a run

## Scope and acceptance

Implements [RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md).

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

This is part of `agent-tab-asks` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
