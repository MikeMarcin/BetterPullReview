---
schema: "implement-rfc.card.v1"
id: "BPR-0587"
title: "Wire RFC-BPR-0023 through the real BetterPullReview composition root and public executable"
status: "Backlog"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0582","BPR-0583","BPR-0585","BPR-0586","BPR-0571"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
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

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Exercise private composition through both native and file-permalink publication
representations, with separate prose/wire digests and one logical receipt.


## M7 implementation prerequisite clarification (mandate r2)

M8 begins only after the coordinator integrates the exact verified M7 local
composition/publication candidate recorded in the [M7 handoff](../../verification/RFC-BPR-0023.md#m7-durable-handoff).
BPR-0584 remains nonterminal until its designated or signed-recorded evidence
and independent public-body/actor read are supplied. That external acceptance
is carried at M9 and blocks RFC closeout; its pending card status does not block
this wiring work. All BPR-0584 completion evidence and RFC criteria 12/13 remain
required. This replaces only the terminal-status prerequisite, preserving the
implementation dependency and the full public-executable requirements above.
