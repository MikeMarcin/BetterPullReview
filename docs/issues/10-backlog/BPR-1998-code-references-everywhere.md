---
schema: "implement-rfc.card.v1"
id: "BPR-1998"
title: "Recognise and open code references in every agent reply"
status: "Backlog"
rfcs: ["RFC-BPR-0101"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0010-code-references-everywhere.md"}
depends_on: []
---

# Recognise and open code references in every agent reply

## Scope and acceptance

Implements [RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md).

Move reference recognition to one core recogniser with the RFC's forms,
placement and resolution rules; use it for the Agent tab, the selected-lines
side chat and the tour side chat; resolve unchanged files of the head and
offer a choice for ambiguous names; draw a link only for a reference that will
open.

Covers AC01 and AC02. If the recogniser's grammar would amount to a general-
purpose parser or regular-expression engine in the core, stop and ask the
owner about a library first, as the repository's rules require.

This is part of `agent-tab-asks` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
