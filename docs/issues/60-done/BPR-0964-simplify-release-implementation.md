---
schema: "implement-rfc.card.v1"
id: "BPR-0964"
title: "Simplify release implementation"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0957","BPR-0963"]
milestone: "M11 Simplification"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a7e8c48b99103ce9b95e4583d92dace6b0795d28","evidence":{"kind":"observation","detail":"Reviewed RFC-0014 maintenance core/host/native/Qt/composition and release tooling using M3-M10 path inventories. Ledger records policy, capability, durable serialization, dependency and incremental-projection findings; unrelated RFC-0036 edits and protected M8 workbench source excluded."}},{"criterion":"AC2","outcome":"pass","revision":"a7e8c48b99103ce9b95e4583d92dace6b0795d28","evidence":{"kind":"observation","detail":"Code-simplifier pass extracts host run_effect from scheduling and shares bounded SHA-256/SHA3-256 stream hashing, replacing one nested archive-suffix expression. Exact diff preserves authority, cancellation/commit/error handling and source/package behavior. No new API, schema, dependency or native capability."}},{"criterion":"AC3","outcome":"pass","revision":"a7e8c48b99103ce9b95e4583d92dace6b0795d28","evidence":{"kind":"command","detail":"Seven maintenance integration tests, four package/source/security contract suites, actual M9 source-asset preflight, complete tracked-candidate architecture gate, documentation gate/64 regressions and scoped board all pass. Changed host build/format/tidy pass on identical bytes. See build/m11-*-evidence.json and docs/verification/RFC-BPR-0014.md#milestone-11-simplification. All downstream native/trust/vulnerability/vault/crash/human/canonical obligations preserved."}}]
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

Implementation candidate `a7e8c48b99103ce9b95e4583d92dace6b0795d28` passes
the seven-test maintenance integration (21.02s), four package/source/security
contract suites (1.89s), exact retained-source preflight, complete tracked-source
architecture gate, documentation gate and 64 documentation regressions. The
changed host and consumers build; format/tidy pass on the identical candidate
translation unit. Scoped board validation reports 957 cards and no errors.

- AC1 passes: the ledger identifies the policy, capability, serialization,
  dependency and projection owners, with source-backed no-change dispositions.
- AC2 passes: the two-file simplification preserves the exact maintenance and
  package contracts. Self-review found no new defect requiring a broader change.
- AC3 passes: affected hostile contracts and coherent public composition pass;
  receipts are `build/m11-*-evidence.json`. The old M9 artifact is a read-only
  verifier input, not a newly qualified M11 binary.

M8/M9 native, human, independent trust, vulnerability, vault/crash and final
RFC/predecessor obligations remain pending. No publication, remote mutation,
candidate-protection release or new attended session is part of M11. M12 remains
ineligible until those prerequisites are accepted.
