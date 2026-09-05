---
schema: "implement-rfc.card.v1"
id: "BPR-0574"
title: "Reconcile RFC-BPR-0022 documentation and simplify the implementation"
status: "Backlog"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0572"]
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

## Import boundary

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
