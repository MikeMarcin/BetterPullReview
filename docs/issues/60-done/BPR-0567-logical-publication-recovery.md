---
schema: "implement-rfc.card.v1"
id: "BPR-0567"
title: "Migrate durable recovery to logical publications and child attempts"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0562","BPR-0564","BPR-0565"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"transactional-migration","outcome":"pass","revision":"d9ce7daf2ca2b3db3cd7ddc72f6d9a261a52a034","evidence":{"kind":"command","detail":"sqlite_review_mutation_recovery_storage and sqlite_review_submission_recovery_storage pass; v1-v3 preservation, both-direction alias refusal, two-connection CAS, durable rejection/evidence, restart, corruption, baseline and 1024 capacity exercised. m2-recovery-tests-alias.log and m2-recovery-tests-final2.log."}},{"criterion":"representation-keys","outcome":"pass","revision":"d9ce7daf2ca2b3db3cd7ddc72f6d9a261a52a034","evidence":{"kind":"command","detail":"sqlite_review_mutation_recovery_storage and sqlite_review_submission_recovery_storage pass; v1-v3 preservation, both-direction alias refusal, two-connection CAS, durable rejection/evidence, restart, corruption, baseline and 1024 capacity exercised. m2-recovery-tests-alias.log and m2-recovery-tests-final2.log."}},{"criterion":"single-post-child","outcome":"pass","revision":"d9ce7daf2ca2b3db3cd7ddc72f6d9a261a52a034","evidence":{"kind":"command","detail":"sqlite_review_mutation_recovery_storage and sqlite_review_submission_recovery_storage pass; v1-v3 preservation, both-direction alias refusal, two-connection CAS, durable rejection/evidence, restart, corruption, baseline and 1024 capacity exercised. m2-recovery-tests-alias.log and m2-recovery-tests-final2.log."}},{"criterion":"atomic-authorization","outcome":"pass","revision":"d9ce7daf2ca2b3db3cd7ddc72f6d9a261a52a034","evidence":{"kind":"command","detail":"sqlite_review_mutation_recovery_storage and sqlite_review_submission_recovery_storage pass; v1-v3 preservation, both-direction alias refusal, two-connection CAS, durable rejection/evidence, restart, corruption, baseline and 1024 capacity exercised. m2-recovery-tests-alias.log and m2-recovery-tests-final2.log."}},{"criterion":"indeterminate-native-refusal","outcome":"pass","revision":"d9ce7daf2ca2b3db3cd7ddc72f6d9a261a52a034","evidence":{"kind":"command","detail":"sqlite_review_mutation_recovery_storage and sqlite_review_submission_recovery_storage pass; v1-v3 preservation, both-direction alias refusal, two-connection CAS, durable rejection/evidence, restart, corruption, baseline and 1024 capacity exercised. m2-recovery-tests-alias.log and m2-recovery-tests-final2.log."}},{"criterion":"closed-recovery-counterexamples","outcome":"pass","revision":"d9ce7daf2ca2b3db3cd7ddc72f6d9a261a52a034","evidence":{"kind":"command","detail":"sqlite_review_mutation_recovery_storage and sqlite_review_submission_recovery_storage pass; v1-v3 preservation, both-direction alias refusal, two-connection CAS, durable rejection/evidence, restart, corruption, baseline and 1024 capacity exercised. m2-recovery-tests-alias.log and m2-recovery-tests-final2.log."}}]
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

## Implementation evidence

Candidate `d9ce7daf2ca2b3db3cd7ddc72f6d9a261a52a034`: both SQLite recovery suites pass. Schema 4 adds a
digest-protected logical parent with at most two children; schemas 1–3 keep their
exact native and whole-review rows. Alias refusal works in both directions,
including migrated succeeded/unresolved rows. Two-connection CAS, restart,
non-creation/evidence authorization, baseline corruption and capacity are covered.
[Acceptance](../_checkpoints/BPR-0575/bpr0567-acceptance.json).
