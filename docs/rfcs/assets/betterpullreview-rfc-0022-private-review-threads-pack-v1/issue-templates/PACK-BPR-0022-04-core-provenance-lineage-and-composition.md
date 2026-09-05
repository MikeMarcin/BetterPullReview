---
provisional_id: PACK-BPR-0022-04
title: Implement Qt-free agent-run provenance, lineage, finding identity, and composition policy
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-03]
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

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
