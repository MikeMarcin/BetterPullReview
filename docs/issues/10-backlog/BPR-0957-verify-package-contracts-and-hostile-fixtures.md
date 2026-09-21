---
schema: "implement-rfc.card.v1"
id: "BPR-0957"
title: "Verify package contracts and hostile fixtures"
status: "Backlog"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0956"]
milestone: "M7 Focused tests and fixtures"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
---

# Verify package contracts and hostile fixtures

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Acceptance

- AC1: Add or reuse focused core replay, host recovery/cancellation, Qt composition, package relocation and hostile boundary fixtures chosen from the changed public graph.
- AC2: Demonstrate meaningful negative controls for missing required packaged capabilities, untrusted plugin/library paths, invalid package identity and source/secret canaries, with real failure on a false success claim.
- AC3: Bind checks to exact candidate and artifact identities; tests-disabled byte/symbol/graph inspection proves fixture seams and undeclared dependencies are absent.

## Verification and execution record

Not run. Imported under mandate r1 at RFC base `d0a8a6dda005af12b3818234f4878a9f48a44146`.
The milestone owner must record source/artifact identities and focused public
checks here. Native, human and external evidence cannot be self-certified.
