---
schema: "implement-rfc.card.v1"
id: "BPR-1954"
title: "Offer explicit replay fallback and preserve legacy histories"
status: "Done"
rfcs: ["RFC-BPR-0096"]
metadata: {"mandate_revision":"native-agent-sessions-r1","reservation":"docs/issues/_reservations/BPR-1951--BPR-1956--ed8ca44cbe3b.json","milestone_plan":{"id":"native-agent-sessions-v1","path":"docs/rfcs/plans/native-agent-sessions-v1/milestone-plan.json"}}
depends_on: ["BPR-1951", "BPR-1952", "BPR-1953"]
acceptance: [{"criterion":"AC06","outcome":"pass","revision":"2de6a3336a02b7aae5a62700d17b620eff98451d","evidence":{"kind":"observation","detail":"Explicit labelled native-source and unknown legacy-source replay generations, native provenance, unsupported New alternative, scoped permissions and restart proven by six public courses plus SQLite/launch negative controls.","references":["m4-public-raster.log","m4-integration-final.log","m4-uncertain-check.log","m4-core-check.log","m4-tidy2.log","m4-tidy3.log","m4-doc1.log","m4-doc-tests.log"]}},{"criterion":"AC05-replay-boundary","outcome":"pass","revision":"2de6a3336a02b7aae5a62700d17b620eff98451d","evidence":{"kind":"observation","detail":"Missing workspace starts no process and never retransmits; selecting replay sends no inference; uncertain replay restart has visible refusal without native handle/workspace and zero effects.","references":["m4-public-raster.log","m4-integration-final.log","m4-uncertain-check.log","m4-core-check.log","m4-tidy2.log","m4-tidy3.log","m4-doc1.log","m4-doc-tests.log"]}},{"criterion":"AC11-compatibility-bounds","outcome":"pass","revision":"2de6a3336a02b7aae5a62700d17b620eff98451d","evidence":{"kind":"observation","detail":"Shared transactional500 native plus legacy admission refuses safely without pruning protected histories, existing saves at capacity work, legacy save failures are visible; core50/8KiB48KiB or12/48KiB replay bounds count omissions and reject oversized whole tour carrier.","references":["m4-public-raster.log","m4-integration-final.log","m4-uncertain-check.log","m4-core-check.log","m4-tidy2.log","m4-tidy3.log","m4-doc1.log","m4-doc-tests.log"]}}]
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
