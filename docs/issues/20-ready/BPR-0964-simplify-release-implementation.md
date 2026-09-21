---
schema: "implement-rfc.card.v1"
id: "BPR-0964"
title: "Simplify release implementation"
status: "Ready"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957","BPR-0963"]
milestone: "M11 Simplification"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Simplify release implementation

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Review the RFC's changed product/package paths for duplicated policy, broad capability access, needless serialization, unnecessary dependency collection and whole-state refreshes.
- AC2: Apply a bounded simplification pass or record evidence-backed no-change findings, preserving the exact authority and package/security contracts.
- AC3: Rerun affected checks after any correction and one coherent integration course; reuse unchanged evidence and keep native/human/external gates explicit.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.
