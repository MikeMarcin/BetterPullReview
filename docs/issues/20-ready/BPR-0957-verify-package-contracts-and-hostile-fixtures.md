---
schema: "implement-rfc.card.v1"
id: "BPR-0957"
title: "Verify package contracts and hostile fixtures"
status: "Ready"
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

## M2 allocation for the successor

Use the [nine-row public-check map](../../architecture/release-readiness.md)
and G1–G9 to bind each negative control to a claimed terminal state. Include
missing packaged helper, untrusted library/plugin origin, mismatched snapshot/
selection, canary content, corrupt/future/foreign stores, partial cleanup and
preserved sibling/unknown data. Tests-disabled packages must contain no fixture
entry; public fixture evidence is not native authentication/trust evidence.

M2's real epoch-maintenance and composed-host checks passed with unchanged
product source. The checkout architecture scan failed only on an ignored nested
unrelated worktree; it passed on an exact tracked-source archive. Preserve that
known scanner/worktree limitation and qualify any future verifier change in this
verification scope; never delete another owner's checkout to make a scan pass.

## M6 accepted handoff

BPR-0956 is Done at guarded finish `2589f07f`. Read the
[M6 handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m6.json)
and [artifact/public-path ledger](../../verification/RFC-BPR-0014.md#milestone-6-composition-and-public-path).
The clean source candidate is `da12f556`; its tests-disabled universal local
package at `build/releases/m6-da12f556/` reached the exact fixture comparison
through native UI and normal Quit. Reuse unchanged focused and artifact evidence.
M7 still owns its hostile/missing-capability controls and comprehensive
tests-disabled seam assertions. The older runtime-surface script forces an
unshipped offscreen plugin; select a native platform for package inspection
without expanding the shipped plugin allowlist. Windows/Linux native and
release-trust/license gates remain M8/M9, not waived by the macOS result.
