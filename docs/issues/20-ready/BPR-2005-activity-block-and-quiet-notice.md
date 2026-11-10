---
schema: "implement-rfc.card.v1"
id: "BPR-2005"
title: "Show what a running turn is doing, its helpers and when it goes quiet"
status: "Ready"
rfcs: ["RFC-BPR-0103"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0017-activity-block-and-quiet-notice.md"}
depends_on: ["BPR-2004"]
---

# Show what a running turn is doing, its helpers and when it goes quiet

## Scope and acceptance

Implements
[RFC-BPR-0103](../../rfcs/RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md).

Present the running block (what the agent is doing, elapsed time, time since
the last frame, helper rows, Stop), fold it under the answer after the turn,
and show the 120-second quiet notice with its two choices, in the Agent tab
and the tour side chat.

Covers AC05, AC06, AC08 and the owner's look in AC09. Load frontend-design
before the Qt work.

This is part of `agent-work-visible` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
