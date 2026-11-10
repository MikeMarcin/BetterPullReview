---
schema: "implement-rfc.card.v1"
id: "DRAFT-0011"
title: "Send on Return and show cost per turn and per session"
status: "Backlog"
rfcs: ["RFC-DRAFT-0004"]
depends_on: []
---

# Send on Return and show cost per turn and per session

## Scope and acceptance

Implements [RFC-DRAFT-0004](../RFC-DRAFT-0004-agent-chat-conventions.md).

Make Return send and Shift+Return add a line in the Agent tab and the
selected-lines side chat, with the input-method and empty-composer exceptions;
hold a turn cost and a session total in the core; establish against the
installed Claude how the runtime's figure maps to a turn's cost and record it;
format and place both as the RFC states, in the Agent tab and the tour side
chat.

Covers AC03 to AC06. Composers that save or publish text keep their keys.

This is part of `agent-tab-asks` in the [plan](../milestone-plan.json).
