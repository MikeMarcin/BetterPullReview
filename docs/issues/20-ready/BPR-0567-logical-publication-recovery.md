---
schema: "implement-rfc.card.v1"
id: "BPR-0567"
title: "Migrate durable recovery to logical publications and child attempts"
status: "Ready"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0562","BPR-0564","BPR-0565"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
---

# Migrate durable recovery to logical publications and child attempts

Migrate the SQLite recovery contract from one exact wire attempt to one logical publication with deterministic native and fallback child attempts.

## Acceptance

- Migration is transactional, versioned, schema-exact, and preserves existing succeeded/unresolved rows.
- Parent and child keys/fingerprints bind representation and contract revision.
- At most one child is POST-eligible at a time.
- Fallback creation atomically records exact native rejection or the no-native-attempt
  unsupported-evidence admission; cached evidence cannot override an unresolved native attempt.
- An indeterminate native state cannot enable fallback; only an exact fully received
  non-creation rejection can resolve its witness into definitive unsupported state.
- Restart, corruption, capacity, conflict, baseline-digest, and legacy-row tests fail closed.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Cover both first-attempt unsupported-evidence admission and exact native-rejection
negotiation. Persist the authorization before a fallback child exists; never
fabricate a rejected native child or authorize fallback from a lost response.
