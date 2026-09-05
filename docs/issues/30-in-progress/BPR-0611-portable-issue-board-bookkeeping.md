---
id: BPR-0611
title: Portable issue-board validation and bookkeeping
status: In Progress
rfc: none
depends_on: []
---

# Portable issue-board validation and bookkeeping

Implement the owner's requested implement-rfc workflow improvement. This is
workflow maintenance, not a product RFC or a change to RFC acceptance.

## Acceptance criteria

- One strict parser and schema-backed card validator serve the queue, mover,
  portable audit, and repository documentation gate. Board validation checks
  identity, RFC references, dependency graphs, and configured lane rules.
- The portable CLI reports RFC-linked work and deterministic readiness without
  inferring scope from prose or changing the board.
- A move preserves unrelated bytes, synchronizes lane metadata, repairs live
  links, protects captured records, and refuses invalid or competing writes.
  Dry runs are read-only; ordinary write failures roll back applied changes.
- The skill routes bookkeeping through the commands; generated adapters agree.

## Verification

Pending focused Python tests, temporary-board CLI exercise, live read-only
validation/queue, documentation gate, adapter drift check, and diff hygiene.
No product build matrix is required for this Python/documentation slice.
