---
schema: "implement-rfc.card.v1"
id: "BPR-0964"
title: "Simplify release implementation"
status: "In Progress"
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

Sole implementer `/root/rfc0014_m11` claimed this card at
`aed1172fdfaa46c0057c16a9bff9fa10724c41f8`, under mandate r3 and
coordinator-implementer rev2. Entry was clean
`484a19807a2b9c541d45c7b0a15e924be9894ff4`; immutable RFC base remains
`d0a8a6dda005af12b3818234f4878a9f48a44146`. The
[M11 binding](../../rfcs/plans/rfc-bpr-0014-release-readiness/checkpoint-m11.json)
retains all three criteria and both authored exits, with accepted M10 as its
prerequisite.

The [simplification ledger](../../verification/RFC-BPR-0014.md#milestone-11-simplification)
records the changed-scope review and source-backed no-change findings. Two
implementation files change: the maintenance host separates effect execution
from completion scheduling; the package tool shares bounded stream hashing
and replaces a nested archive-suffix expression with explicit branches.
Authority, cancellation/commit ordering, errors, selected dependencies, installed
notices and the separate source-asset contract are preserved.

The pre-change composed public path passes. Preliminary changed-surface build,
format/tidy and all 19 package/source tests pass. Exact-candidate focused
integration and final documentation/board/checkpoint acceptance remain to be
recorded before Done. M8/M9 native, human, independent trust, vulnerability,
vault/crash and final RFC/predecessor obligations remain pending. No publication,
remote mutation, candidate-protection release or new attended session is part
of M11.
