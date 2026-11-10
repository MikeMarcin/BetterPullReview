---
schema: "implement-rfc.card.v1"
id: "BPR-1999"
title: "Send on Return and show cost per turn and per session"
status: "In Progress"
rfcs: ["RFC-BPR-0101"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0011-send-key-and-cost.md"}
depends_on: []
---

# Send on Return and show cost per turn and per session

## Scope and acceptance

Implements [RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md).

Make Return send and Shift+Return add a line in the Agent tab and the
selected-lines side chat, with the input-method and empty-composer exceptions;
hold a turn cost and a session total in the core; establish against the
installed Claude how the runtime's figure maps to a turn's cost and record it;
format and place both as the RFC states, in the Agent tab and the tour side
chat.

Covers AC03 to AC06. Composers that save or publish text keep their keys.

This is part of `agent-tab-asks` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
