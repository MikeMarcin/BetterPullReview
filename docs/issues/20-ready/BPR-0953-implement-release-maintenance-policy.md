---
schema: "implement-rfc.card.v1"
id: "BPR-0953"
title: "Implement release maintenance policy"
status: "Ready"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0952"]
milestone: "M3 Deterministic functional core"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Implement release maintenance policy

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Implement or directly prove reusable deterministic retention, purge, migration and diagnostic-selection/redaction policy with compact state and explicit operation identity.
- AC2: Cover permitted/refused/destructive/stale transitions with direct core tests that require no QApplication, Qt, provider, clock, filesystem, thread or other effects.
- AC3: Record any no-impact core region with exact existing source/test evidence; do not add policy merely to fill a plan stage.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.

## M2 allocation for the successor

The [source allocation](../../architecture/release-readiness.md) is the r3
starting point. Reuse the epoch maintenance transition/identity and each
store's migration authority; qualify them directly instead of creating a
generic migration engine. New core work is compact diagnostic snapshot
selection/preview-export identity, clearing, and exact profile-purge eligibility
(G5/G6). Define bounds, allowed categories, confirmation/cancellation and stale/
duplicate refusals with no native path as authority. M4 owns all observations
and effects. RFC-0027 is a compatibility reference, not authorization to activate
its deferred BPR-0603 or optional broad logging features. No new policy is
implemented or accepted by M2.
