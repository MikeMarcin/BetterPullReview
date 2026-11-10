---
schema: "implement-rfc.card.v1"
id: "BPR-1995"
title: "Clear the side chat foot, answer change requests with the change and run Enhance in the conversation"
status: "Backlog"
rfcs: ["RFC-BPR-0100"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0007-tour-chat-foot-and-one-turn-changes.md"}
depends_on: ["BPR-1999", "BPR-2000"]
---

# Clear the side chat foot, answer change requests with the change and run Enhance in the conversation

## Scope and acceptance

Implements
[RFC-BPR-0100](../../rfcs/RFC-BPR-0100-tour-conversation-that-changes-the-tour-in-one-turn.md).

Reduce what sits under the transcript to the composer and the session line;
show the update link under any answer that proposed nothing, in every thread,
as the only place the update is offered; make a change request in the whole-
tour thread yield its revision in the same turn, shown in the chat as a card
with Accept all, Review each in the tour and Discard; route the application's
Enhance buttons through the part's side chat session.

Covers AC01 to AC05 and the owner's look in AC09. The session line's cost and
the Accept buttons' ordinary role come from RFC-BPR-0101. Sessions, history
and replay under RFC-BPR-0096 do not change.

This is part of `tour-conversation` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
