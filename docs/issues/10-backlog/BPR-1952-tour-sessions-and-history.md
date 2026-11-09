---
schema: "implement-rfc.card.v1"
id: "BPR-1952"
title: "Add per-agent tour sessions and New session history"
status: "Backlog"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: ["BPR-1951"]
---

# Add per-agent tour sessions and New session history

## Scope and acceptance

Extend the [RFC's](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md)
native host to the whole-tour, Problem, Solution and stable-stop threads,
separately per agent. Share the whole-tour native conversation with initial
tour drafting. Add scoped durable history and New session behavior in both
surfaces without Clear's destructive confirmation, with accepted work and
pending proposal identity preserved. Cover failed retention, history selection,
removed stops and Reset tour/Undo reset associations. Qualify the tour's
native permission configuration; keep tool-free hooks separate. This is the `tour-sessions` milestone in the
[plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Cover the tour portions of AC01–AC03, New session/history in AC04, and the
tour/new-generation portions of AC05. Exercise stop removal/replacement,
reordering and agent switching without inheriting unrelated conversations.
Rendered control checks contribute to AC10; installed qualification remains
on its own proposed card.
