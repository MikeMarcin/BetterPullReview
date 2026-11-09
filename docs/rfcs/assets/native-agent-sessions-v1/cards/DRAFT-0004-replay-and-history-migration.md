---
schema: "implement-rfc.card.v1"
id: "DRAFT-0004"
title: "Offer explicit replay fallback and preserve legacy histories"
status: "Backlog"
rfcs: ["RFC-DRAFT-0030"]
depends_on: ["DRAFT-0001", "DRAFT-0002", "DRAFT-0003"]
---

# Offer explicit replay fallback and preserve legacy histories

## Scope and acceptance

Implement the [RFC's](../RFC-DRAFT-0030-native-agent-sessions-and-tour-context.md)
capability distinction, unavailable-native recovery, labelled Transcript
replay generations and legacy-history migration without assigning an ambiguous
numeric agent slot to a new entry. Preserve native provenance, New session as
the alternative and existing context bounds/omission reporting. Replay does
not restore workspace modifications or bypass permission checks. Native
failure never automatically resends a question through replay. This contributes to
`qualified-continuation` in the [plan](../milestone-plan.json).

Cover AC06, replay-boundary portions of AC05, and relevant compatibility and
boundedness portions of AC11. Use a nonresuming custom-harness fixture,
missing-native-state course and a legacy stored conversation to distinguish
the cases through the actual product path.
