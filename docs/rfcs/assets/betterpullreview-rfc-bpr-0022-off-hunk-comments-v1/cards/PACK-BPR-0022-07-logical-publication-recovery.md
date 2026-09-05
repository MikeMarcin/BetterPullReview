---
provisional_id: PACK-BPR-0022-07
live_id: allocate-on-import
title: Migrate durable recovery to logical publications and child attempts
status: Backlog
rfc: RFC-BPR-0022
depends_on: [PACK-BPR-0022-02, PACK-BPR-0022-04, PACK-BPR-0022-05]
---

# Migrate durable recovery to logical publications and child attempts

Migrate the SQLite recovery contract from one exact wire attempt to one logical publication with deterministic native and fallback child attempts.

## Acceptance

- Migration is transactional, versioned, schema-exact, and preserves existing succeeded/unresolved rows.
- Parent and child keys/fingerprints bind representation and contract revision.
- At most one child is POST-eligible at a time.
- Fallback child creation is an atomic transition from definitive native unsupported rejection only.
- No possibly-transmitted native state can create or enable fallback.
- Restart, corruption, capacity, conflict, baseline-digest, and legacy-row tests fail closed.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.
