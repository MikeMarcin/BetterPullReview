---
schema: "implement-rfc.card.v1"
id: "DRAFT-0010"
title: "Recognise and open code references in every agent reply"
status: "Backlog"
rfcs: ["RFC-DRAFT-0004"]
depends_on: []
---

# Recognise and open code references in every agent reply

## Scope and acceptance

Implements [RFC-DRAFT-0004](../RFC-DRAFT-0004-agent-chat-conventions.md).

Move reference recognition to one core recogniser with the RFC's forms,
placement and resolution rules; use it for the Agent tab, the selected-lines
side chat and the tour side chat; resolve unchanged files of the head and
offer a choice for ambiguous names; draw a link only for a reference that will
open.

Covers AC01 and AC02. If the recogniser's grammar would amount to a general-
purpose parser or regular-expression engine in the core, stop and ask the
owner about a library first, as the repository's rules require.

This is part of `agent-tab-asks` in the [plan](../milestone-plan.json).
