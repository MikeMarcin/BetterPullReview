---
schema: "implement-rfc.card.v1"
id: "BPR-1953"
title: "Give tour sessions authoritative state and source-status reasons"
status: "Backlog"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: ["BPR-1952"]
---

# Give tour sessions authoritative state and source-status reasons

## Scope and acceptance

Implement the [RFC's](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md)
versioned BPR tour guide, structured snapshot and current-state updates in
native and replay contexts. Refresh on all relevant BPR fact changes and
unestablished bootstrap after compaction/reconnect, only with explicit sends.
Derive distinct source confirmation, preparation, mapping, reaffirmation,
Ready/publication eligibility and corrective actions from core facts shared
with Qt, including whole-tour blockers and captured stable stop identities.
Correct only the bounded misleading warning behavior specified in the RFC;
preserve anchor identity on text/order edits and publication/acceptance
boundaries. This is the `tour-context` milestone in the
[plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Cover AC08 and AC09 and the tour-state-update portion of AC07. A composed
warning/update course contributes to AC10. Retain the distinction between
source diagnosis and the owner's not-yet-reproduced exact sequence.
