---
schema: "implement-rfc.card.v1"
id: "BPR-1954"
title: "Offer explicit replay fallback and preserve legacy histories"
status: "Backlog"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: ["BPR-1951", "BPR-1952", "BPR-1953"]
---

# Offer explicit replay fallback and preserve legacy histories

## Scope and acceptance

Implement the [RFC's](../../rfcs/RFC-BPR-0096-native-agent-sessions-and-tour-context.md)
capability distinction, unavailable-native recovery, labelled Transcript
replay generations and legacy-history migration without assigning an ambiguous
numeric agent slot to a new entry. Preserve native provenance, New session as
the alternative and existing context bounds/omission reporting. Replay does
not restore workspace modifications or bypass permission checks. Native
failure never automatically resends a question through replay. This contributes to
`qualified-continuation` in the [plan](../../rfcs/plans/native-agent-sessions-v1/milestone-plan.json).

Cover AC06, replay-boundary portions of AC05, and relevant compatibility and
boundedness portions of AC11. Use a nonresuming custom-harness fixture,
missing-native-state course and a legacy stored conversation to distinguish
the cases through the actual product path.
