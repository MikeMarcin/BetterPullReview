---
schema: "implement-rfc.card.v1"
id: "DRAFT-0003"
title: "Give tour sessions authoritative state and source-status reasons"
status: "Backlog"
rfcs: ["RFC-DRAFT-0030"]
depends_on: ["DRAFT-0002"]
---

# Give tour sessions authoritative state and source-status reasons

## Scope and acceptance

Implement the [RFC's](../RFC-DRAFT-0030-native-agent-sessions-and-tour-context.md)
versioned BPR tour guide, structured snapshot and current-state updates in
native and replay contexts. Refresh on all relevant BPR fact changes and
unestablished bootstrap after compaction/reconnect, only with explicit sends.
Derive distinct source confirmation, preparation, mapping, reaffirmation,
Ready/publication eligibility and corrective actions from core facts shared
with Qt, including whole-tour blockers and captured stable stop identities.
Correct only the bounded misleading warning behavior specified in the RFC;
preserve anchor identity on text/order edits and publication/acceptance
boundaries. This is the `tour-context` milestone in the
[plan](../milestone-plan.json).

Cover AC08 and AC09 and the tour-state-update portion of AC07. A composed
warning/update course contributes to AC10. Retain the distinction between
source diagnosis and the owner's not-yet-reproduced exact sequence.
