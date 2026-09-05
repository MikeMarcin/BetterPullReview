---
schema: "implement-rfc.card.v1"
id: "BPR-0587"
title: "Wire RFC-BPR-0023 through the real BetterPullReview composition root and public executable"
status: "Backlog"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0582","BPR-0583","BPR-0584","BPR-0585","BPR-0586","BPR-0571"]
milestone: "Post-Pack v2"
---

# Wire RFC-BPR-0023 through the real BetterPullReview composition root and public executable

## Objective

Prove the feature is selected, reachable, durable, and integrated rather than living only in isolated components.

## Required final state

- Production store ports, private-thread host, run metadata, composition host, Qt projections, existing invocation host, draft editor, and mutation host are wired in `src/app`.
- A deterministic public-path test creates a private reviewer turn, receives an agent turn with provenance, restarts, composes, accepts a draft, proves no mutation, then separately publishes through a safe fixture.
- Normal GitHub conversation synchronization remains functional before and after local composition/publication lineage updates.
- Application shutdown cancels/joins bounded work and leaves committed private state recoverable.
- Core-only and tests-disabled configurations retain correct dependency boundaries or documented no-impact behavior.

## Non-goals

- An alternate demo executable as sole evidence
- Provider credentials in test composition
- Bypassing existing hosts to simplify wiring

## Completion evidence

- Composition-root code path
- Public executable/end-to-end test output
- Restart and shutdown traces
- Build matrix results

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04; implementation is deferred by the user. This card remains
Backlog. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Exercise private composition through both native and file-permalink publication
representations, with separate prose/wire digests and one logical receipt.
