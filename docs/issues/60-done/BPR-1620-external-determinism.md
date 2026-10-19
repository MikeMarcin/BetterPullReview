---
schema: "implement-rfc.card.v1"
id: "BPR-1620"
title: "Make external determinism the general core rule"
status: "Done"
rfcs: ["RFC-BPR-0001"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"external-determinism-r1","RFC_BASE_SHA":"80f280ee8c9946b98c6527a79c65c878d0a6a4d1","reservation":"docs/issues/_reservations/BPR-1620--BPR-1620--6288804bbf37.json","milestone_plan":{"id":"external-determinism-v1","path":"docs/rfcs/plans/external-determinism-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4773277f981d429c29862e297c0e31c3d5e90335","evidence":{"kind":"artifact","path":"docs/issues/_checkpoints/BPR-1620/verification.md","detail":"Authoritative external-determinism contract and linked live guidance; current RFC amendment is adopted without rewriting history."}},{"criterion":"AC2","outcome":"pass","revision":"4773277f981d429c29862e297c0e31c3d5e90335","evidence":{"kind":"artifact","path":"docs/issues/_checkpoints/BPR-1620/verification.md","detail":"Implementation constraints, host effect/lifecycle ownership, Qt independence and resource/cross-platform qualifications preserved; no behavior/dependency changes."}},{"criterion":"AC3","outcome":"pass","revision":"4773277f981d429c29862e297c0e31c3d5e90335","evidence":{"kind":"artifact","path":"docs/issues/_checkpoints/BPR-1620/verification.md","detail":"Documentation gate, 67 docs contracts, both direct boundary scans, 11 core and ten focused architecture contracts pass; CMake executable content unchanged."}}]
---

# Make external determinism the general core rule

Owner mandate, 2026-10-18: “Let's clean up the architecture docs so external
determinism is the general rule, rather than accumulating special exceptions
for each library.” Mandate `external-determinism-r1`.

## Acceptance

- **AC1:** Define one authoritative external-determinism rule: equal explicit
  inputs, state and options produce equal semantic results, identities, ordering
  and diagnostics under the specified contract. Link live architecture,
  contributor/build guidance and root/core instructions to it; remove the
  library-by-library exceptions from the general definition.
- **AC2:** Preserve effect and host scheduling/lifecycle/cancellation ownership,
  Qt independence, dependency approval/pinning and private wrappers; retain dtl
  bounds, SHA-1 collision detection, xdiff dynamic replacement/licensing and RE2
  no-worker/diagnostic limitations. Qualify resource failure, cancellation and
  cross-platform claims. Preserve original owner answers, historical evidence
  and outstanding native/release acceptance.
- **AC3:** Documentation gate and contract tests, both focused architecture
  boundary checks and their contracts pass against the policy-only candidate.
  Record self-review, exact revisions, intentional commits and command quiescence.
