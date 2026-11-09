---
schema: "implement-rfc.card.v1"
id: "BPR-1951"
title: "Continue native sessions in the Agent tab"
status: "In Progress"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: []
---

# Continue native sessions in the Agent tab

## Scope and acceptance

Implement the [RFC's](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md)
native transports, scoped identities, session-owned workspaces, durable
resume, stale-revision choices and per-generation cancellation/completion
boundaries. Separate durable conversation identity from window activation,
retain ownership through startup recovery, handle uncertain sends without
retransmission and refuse admission when storage cannot be reclaimed safely.
Keep ordinary Agent-tab permissions and own-sign-in behavior.
Qualify both supported native backend families rather than introducing a
prose-only substitute. This is the `native-agent-tab` milestone in the
[plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Cover the Agent-tab portions of AC01–AC03 and AC05, the native-session
portion of AC07, and the resource/authority requirements of AC11. Evidence
for cross-surface controls, tour context and installed-agent qualification is
completed by the dependent proposed cards; partial evidence does not accept
the full RFC.
