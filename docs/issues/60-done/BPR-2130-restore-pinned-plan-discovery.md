---
schema: "implement-rfc.card.v1"
id: "BPR-2130"
title: "Preserve BPR-2120 proof bindings in the pinned plan schema"
status: "Done"
rfcs: []
depends_on: ["BPR-2120"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2130--BPR-2130--d2b7f1790678.json","mandate_revision":"bpr2130-future-main-plan-schema-r1","related_cards":["BPR-2120"],"milestone_plan":{"id":"bpr-2130-plan-schema-proof","path":"docs/rfcs/plans/bpr-2130-plan-schema-proof/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"03568b2fb0b23082d39470d549b1ed89c5e861f0","evidence":{"kind":"artifact","detail":"Pinned965 explicit2120 plan check now exit0/structural valid. Both original exact proof names/SHA values retained in existing allowed boundary text. Restoring original boundary/two keys reconstructs every old JSON value; Done2120 full card/acceptance SHA unchanged. Source metadata correction03568b2fb0b23082d39470d549b1ed89c5e861f0."}},{"criterion":"AC2","outcome":"pass","revision":"03568b2fb0b23082d39470d549b1ed89c5e861f0","evidence":{"kind":"command","detail":"Actual pinned target discovery2120 and independent2129 before: exit2 unknown proof-hash keys; after: exit2 independent release-v041 root amendments. Own error removed, corrected focused2120 and own2130 checks pass. Foreign plan explicitcheck reproduces amendments error and its SHA equals pre-work8832500 source; not modified. Bare absent0 receives no global validation credit."}},{"criterion":"AC3","outcome":"pass","revision":"03568b2fb0b23082d39470d549b1ed89c5e861f0","evidence":{"kind":"command","detail":"Scoped documentation and owned2130 plan pass; explicit metadata-only commits on canonical main preserve foreign work. No R/W/source60/cohort/artifact, runtime, sourceCPP/test, native job, qualification, publication or Build60 intake change. This follow-up is future Main only; global foreign failure remains recorded."}}]
---

# Preserve BPR-2120 proof bindings in the pinned plan schema

The pinned MASK965 schema rejects `baseline_source_sha256` and
`existing_probe_receipt_sha256` as unknown milestone keys in BPR-2120's plan.
Its actual target discovery and discovery for independent BPR-2129 both exit2
on those fields. The bare no-target command returns `absent` and does not
exercise the affected lookup.

Move the two exact names and hash values into the existing allowed `boundary`
string, then remove the unsupported keys. Keep every scope, outcome, work
selector, dependency and exit unchanged. BPR-2120 remains Done with its original
acceptance. This is future-main workflow metadata; release Build60 remains frozen.

## Acceptance

- AC1: The corrected BPR-2120 plan validates with the actual pinned MASK965 schema. Both proof names/hash values remain exact in its existing boundary text; every other plan value and the completed BPR-2120 card/acceptance remain unchanged.
- AC2: Run actual focused validation and target discovery for BPR-2120 and independent BPR-2129 before and after. Verify the own schema error is removed; report any independent foreign-plan error separately without repairing it or crediting global readiness.
- AC3: Documentation and the new owned plan validate, then land only this metadata correction and own tracker/proof paths on current main. Preserve foreign work, the pinned runtime and all release60 source/cohort/artifacts; no source/test/native/qualification change or release intake.

## Verification

[Exact hash preservation and actual discovery outcomes](../../verification/BPR-2130-plan-schema.md).
Global target discovery still reports the separate unchanged release-v041 amendments field; this card does not claim global readiness.
