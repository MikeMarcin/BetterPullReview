---
schema: "implement-rfc.card.v1"
id: "BPR-0572"
title: "Prove RFC-BPR-0022 automated recovery and security invariants"
status: "Backlog"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0571"]
---

# Prove RFC-BPR-0022 automated recovery and security invariants

Run and extend the complete automated hostile matrix across core, host, GitHub, SQLite, Qt, composition, and isolation boundaries.

## Acceptance

- Every decision-table cell and illegal observation is covered.
- Death/loss before and after every baseline, witness, rejection, fallback, receipt, and resolution transition leaves at most one remote candidate.
- Generic 422 and every ambiguous outcome prove zero fallback POST.
- Database-loss, mixed-client edit/reply/resolve/delete, invalid permalink, and uninstall-shaped reconstruction pass.
- Mutation-isolation positive counterexamples include the new fallback operation.
- Focused composition selectors and one direct selected/public path pass at the same head.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

## Import boundary

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
