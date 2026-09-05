---
provisional_id: PACK-BPR-0022-12
title: Wire RFC-BPR-0022 through the real BetterPullReview composition root and public executable
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-07, PACK-BPR-0022-08, PACK-BPR-0022-09, PACK-BPR-0022-10, PACK-BPR-0022-11]
---

# Wire RFC-BPR-0022 through the real BetterPullReview composition root and public executable

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

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
