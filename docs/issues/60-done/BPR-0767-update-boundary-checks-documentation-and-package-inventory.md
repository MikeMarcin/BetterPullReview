---
schema: "implement-rfc.card.v1"
id: "BPR-0767"
title: "Update boundary checks, documentation and package inventory"
status: "Done"
rfcs: ["RFC-BPR-0030", "RFC-BPR-0031"]
depends_on: ["BPR-0761", "BPR-0764"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"verification-closeout","acceptance":["A6","A8","B1","B12"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Core-only and tool isolation remain strict while selected Qt graphs require their resources","outcome":"pass","revision":"fe12d5a9","evidence":{"kind":"command","detail":"Boundary verifier requires exactly ten embedded strips in dev/app-release, excludes source/review/tool inventory, and found no Dif bytes or Qt motion code in core-only."}},{"criterion":"Source/generated/provenance/resource/license disposition and maintainer commands agree","outcome":"pass","revision":"fe12d5a9","evidence":{"kind":"artifact","detail":"Product, architecture, appearance, security, build/test and packaging docs record the accepted source/generated inventory, notice_required false, selected resources and installable-package limitation."}},{"criterion":"Documentation gate and focused regression selectors pass without blanket suppression","outcome":"pass","revision":"fe12d5a9","evidence":{"kind":"command","detail":"DOCUMENTATION_OK, 64 documentation tests, coding-format-check, qt_motion_observer and the measured composed public path passed."}}]
---

# BPR-0767: Update boundary checks, documentation and package inventory

## Outcome

Update selected-graph guards and live docs for the admitted Qt resources, current preference authority, ten-clip contract, skill context and no-runtime-art-tool boundary. Preserve historical packets.

## Acceptance

- Core-only and tool isolation remain strict while selected Qt graphs require their resources.
- Source/generated/provenance/resource/license disposition and maintainers' commands agree.
- Documentation gate and focused regression selectors pass without blanket suppression.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
