---
schema: "implement-rfc.card.v1"
id: "BPR-0579"
title: "Implement Qt-free agent-run provenance, lineage, finding identity, and composition policy"
status: "Ready"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0578"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
---

# Implement Qt-free agent-run provenance, lineage, finding identity, and composition policy

## Objective

Make provenance honesty, immutable derivation, source admission, and Squash-to-Draft legality properties of the core.

## Required final state

- One durable run identity exists conceptually per invocation and is distinct from session identity.
- Requested, configured, protocol-reported, and content-self-reported model/effort facts are separate optional values with conflict diagnostics.
- Finding sources use run + ordinal + digest and preserve validation/revision facts.
- Typed derivation edges are scoped, exact, immutable, and cycle-refused.
- Composition modes cover reviewer-authored, exact copy, and explicit agent-assisted proposal.
- Source digest/currentness/completion/scope checks and exact target partitioning are deterministic.
- There is no legal core transition from composition to published state; accepting yields local draft intent only.
- Duplicate source/body/target publication produces an advisory, not an implicit block or action.

## Non-goals

- Choosing a model for the reviewer
- AI synthesis implementation
- Network mutation

## Completion evidence

- Core test matrix for full/partial/conflicting/unknown provenance
- Cycle and cross-review rejection tests
- Multi-target split tests
- Compile/link boundary evidence

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
