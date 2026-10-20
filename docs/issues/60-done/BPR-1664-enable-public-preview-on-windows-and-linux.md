---
schema: "implement-rfc.card.v1"
id: "BPR-1664"
title: "Enable public preview on Windows and Linux"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0314", "BPR-0315", "BPR-0316"]
priority: "P1"
metadata: {"mandate_revision":"public-preview-platforms-r1","rfc_base_sha":"12ef1db532f78f25ceaa821a5a7bb2fcda001aa2","milestone_plan":{"id":"public-preview-platforms-r1","path":"docs/rfcs/plans/public-preview-platforms-r1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1a7d74f003b656469f5c6822c655a314fb58f663","evidence":{"kind":"observation","detail":"Native tests-disabled Windows/Linux artifacts report enabled GitHub.com and refused GHES, zero request/header/vault/sign-in effects; source/artifact digests and reused product-equivalent Release provenance in ledger.","references":["docs/verification/RFC-BPR-0017.md#platform-expansion-r1","docs/rfcs/plans/public-preview-platforms-r1/handoff.md"]}},{"criterion":"AC2","outcome":"pass","revision":"1a7d74f003b656469f5c6822c655a314fb58f663","evidence":{"kind":"observation","detail":"Final native Windows18/18 and Linux20/20 applicable selectors pass. Both production-factory public reads accept same snapshot/head through14 HTTP200s, zero credential effects, single attempt.","native_live":{"linux":{"wire_requests":14,"candidate":"MikeMarcin/BPR-Test#1 17990ba0c2fb50338141f35c0717f0f99639c7ba","budget":"60/46/1792472127"},"windows":{"wire_requests":14,"candidate":"MikeMarcin/BPR-Test#1 17990ba0c2fb50338141f35c0717f0f99639c7ba","budget":"60/32/1792472127"}},"references":["docs/verification/RFC-BPR-0017.md#platform-expansion-r1","docs/rfcs/plans/public-preview-platforms-r1/handoff.md"]}},{"criterion":"AC3","outcome":"pass","revision":"1a7d74f003b656469f5c6822c655a314fb58f663","evidence":{"kind":"observation","detail":"Mac20 selectors and final six app checks pass; final composition integration4/4, format22/tidy22, documentation gate/67 tests pass. Native build roots and task removed after quiescence; shared caches, clean native checkouts, leases and unrelated ctest preserved. No release/push/publication.","references":["docs/verification/RFC-BPR-0017.md#platform-expansion-r1","docs/rfcs/plans/public-preview-platforms-r1/handoff.md"]}}]
---

# Enable public preview on Windows and Linux

Owner mandate and acceptance: [platform amendment r1](../../rfcs/amendments/RFC-BPR-0017-public-preview-platforms-r1.md).

## Acceptance

- AC1: Native Windows and Linux tests-disabled enabled activation and GHES refusal,
  zero request/header/vault/sign-in effects.
- AC2: Both applicable anonymous native course selector sets and capped
  production-factory public reads pass, retaining all authority refusals.
- AC3: macOS regression, truthful documentation, focused gates, candidate-bound
  evidence and quiescent committed handoff.

## Verification and execution record

Native Windows18/18 and Linux20/20 applicable selectors pass. Both tests-disabled
artifacts pass enabled activation and GHES refusal with zero effects. Final
native public reads each accept the same public snapshot through14 HTTP200s,
with zero credential/header/vault/sign-in effects and single-attempt=true.
Mac regression, focused composition integration, format/tidy and documentation
checks pass. Full identities, failure dispositions, authority limits and native
qualification bounds: [verification ledger](../../verification/RFC-BPR-0017.md#platform-expansion-r1).

Existing release qualification gaps remain outside this slice. No release,
push, publication or remote mutation. No human review criterion was added.
