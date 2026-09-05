---
provisional_id: PACK-BPR-0022-14
live_id: allocate-on-import
title: Reconcile RFC-BPR-0022 documentation and simplify the implementation
status: Backlog
rfc: RFC-BPR-0022
depends_on: [PACK-BPR-0022-12]
---

# Reconcile RFC-BPR-0022 documentation and simplify the implementation

Update the owning product, architecture, provider, review-round-trip, diff, UX, security, verification, fixture, and operator documents, then remove duplicated or representation-leaking machinery.

## Acceptance

- Semantic anchor versus remote subject and the fallback state machine are documented once at their owning boundaries.
- Capability scope, classifier revision, migration, cancellation, limits, mixed-client, database-loss, and support diagnostics are explicit.
- No stale document implies all line comments are native or that generic 422 authorizes fallback.
- Simplification removes duplicated policy, broad capability access, full-state resets, and unnecessary parsing on paint/scroll paths.
- Focused tests, documentation structure gate, and `git diff --check` pass after simplification.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.
