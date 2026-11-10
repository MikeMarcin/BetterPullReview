---
schema: "implement-rfc.card.v1"
id: "DRAFT-0007"
title: "Clear the side chat foot, answer change requests with the change and run Enhance in the conversation"
status: "Backlog"
rfcs: ["RFC-DRAFT-0003"]
depends_on: ["DRAFT-0011", "DRAFT-0012"]
---

# Clear the side chat foot, answer change requests with the change and run Enhance in the conversation

## Scope and acceptance

Implements
[RFC-DRAFT-0003](../RFC-DRAFT-0003-tour-conversation-that-changes-the-tour-in-one-turn.md).

Reduce what sits under the transcript to the composer and the session line;
show the update link under any answer that proposed nothing, in every thread,
as the only place the update is offered; make a change request in the whole-
tour thread yield its revision in the same turn, shown in the chat as a card
with Accept all, Review each in the tour and Discard; route the application's
Enhance buttons through the part's side chat session.

Covers AC01 to AC05 and the owner's look in AC09. The session line's cost and
the Accept buttons' ordinary role come from RFC-DRAFT-0004. Sessions, history
and replay under RFC-BPR-0096 do not change.

This is part of `tour-conversation` in the [plan](../milestone-plan.json).
