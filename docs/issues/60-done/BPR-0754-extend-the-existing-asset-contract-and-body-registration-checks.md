---
schema: "implement-rfc.card.v1"
id: "BPR-0754"
title: "Extend the existing asset contract and body registration checks"
status: "Done"
rfcs: ["RFC-BPR-0030"]
depends_on: ["BPR-0752", "BPR-0753"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"intake-contract","acceptance":["A4","A5","A6"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Closed schema-2 catalog and bounded geometry","outcome":"pass","revision":"df3ee35ef8017e9220c7c7da671702fbc26d64b2","evidence":{"kind":"command","detail":"Synthetic ten-ID/71-frame catalog generated and compiled; fixed count/timing/return/priority constraints and 19-MiB content/24-MiB complete decoded limits including twenty gutters passed 55 focused tests and registered dev CTest asset contract."}},{"criterion":"Adversarial registration, inventory and padding","outcome":"pass","revision":"df3ee35ef8017e9220c7c7da671702fbc26d64b2","evidence":{"kind":"command","detail":"Focused negatives reject displaced body/head/each foot, false annotations, missing content, broad envelopes, wrong gutter metadata/pixels, invalid paths/IDs/counts/returns and overbudget values. Pixel-backed anchors and exact measured per-clip content unions are in compiler ledger; production artwork remains unaccepted."}},{"criterion":"Portable regeneration and actual compressed-byte digests","outcome":"pass","revision":"df3ee35ef8017e9220c7c7da671702fbc26d64b2","evidence":{"kind":"command","detail":"Full verify accepts recompressed canonical PNG with refreshed actual-payload digest, rejects stale payload hashes, and preserves exact text metadata. Shipped schema-1 validate/verify passed unchanged, 55-test Python suite passed, dev CTest dif_assets_contract passed in 43.28s, documentation gate returned DOCUMENTATION_OK and git diff --check passed."}}]
---

# BPR-0754: Extend the existing asset contract and body registration checks

## Outcome

Design/implement schema-2 ten-clip support in the existing compiler, exact inventories, body anchors separate from limb envelopes, fixed gutter handling and revised limits. Start with synthetic unit fixtures while art is normalized.

## Acceptance

- Closed IDs/checked counts, frame times, geometry and 24-MiB complete limit are enforced.
- Displaced feet, missing content, wrong padding, malformed returns and unknown IDs fail focused tests.
- Canonical-pixel regeneration and actual-byte digest semantics survive compressor differences.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.

## Execution record

- Compiler/test revision: `df3ee35ef8017e9220c7c7da671702fbc26d64b2`.
  Requirement bytes before this section remain unchanged;
  SHA-256 `89b889c33a2c16833d4d20bbdc67a700204513116caf221eca0ec2b53c9b2d91`.
- Implemented schema-2 ordered ten-clip/count/timing checks, exact inventories,
  measured content unions, pixel-backed stable body/foot/head registration,
  neutral returns, gutter geometry and descriptor RGBA digests. Provenance keeps
  its independent schema-1 shape. Shipped schema-1 artifacts are unchanged.
- Direct synthetic asset course: `PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/dif_asset_tests.py -v`
  passed 55 tests in 41.728 seconds. The additional vertical-foot/named-anchor
  cases passed with `PYTHONDONTWRITEBYTECODE=1 python3 tests/scripts/dif_asset_tests.py SchemaTwoTests.test_rejects_displaced_body_and_each_foot_with_unchanged_annotations -v`
  in 7.049 seconds; compiler unchanged since the full pass.
- Live schema-1 commands `PYTHONDONTWRITEBYTECODE=1 python3 scripts/dif_assets.py validate --assets src/adapters/qt/assets/dif`
  and `PYTHONDONTWRITEBYTECODE=1 python3 scripts/dif_assets.py verify --assets src/adapters/qt/assets/dif`
  passed. Both preserve portable canonical pixels and actual compressed-payload digests.
- `PYTHONDONTWRITEBYTECODE=1 python3 scripts/verify_documentation.py --root .`
  returned `DOCUMENTATION_OK`; `git diff --check` passed.
- `PYTHONDONTWRITEBYTECODE=1 ctest --preset dev -R '^dif_assets_contract$' --output-on-failure`
  passed the registered contract in 43.28 seconds on the committed compiler/test revision.
- Fixture: 71 content cells plus twenty gutters = 23,855,104 decoded bytes
  (22.75 MiB). The ceiling is 24 MiB complete, 19 MiB content, 76 content frames,
  24 frames/clip, 12 fps, 2048 source dimension and 16 MiB/source file.
- [Compiler contract evidence](../../verification/RFC-BPR-0030-compiler-contract.md)
  records schema fields, exact source hashes, direct negative controls and limitations.
  Synthetic geometry does not establish real artwork identity, rights, correct
  choreography or attended playback acceptance. Those later art obligations remain open.
