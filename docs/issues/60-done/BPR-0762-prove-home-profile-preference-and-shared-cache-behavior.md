---
schema: "implement-rfc.card.v1"
id: "BPR-0762"
title: "Prove Home, profile preference and shared-cache behavior"
status: "Done"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0761"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"home-integration","acceptance":["B2","B3","B5","B9","B10","B11"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"B2","outcome":"pass","revision":"b299818e36887bdf3d4e8be138ed08095ca3e662","evidence":{"kind":"observation","detail":"docs/verification/RFC-BPR-0031-home-integration.md: actual production greeting and breathing widget renders differ, four calm loops reach blink, and reduced motion remains on rest without a wakeup."}},{"criterion":"B3","outcome":"pass","revision":"b299818e36887bdf3d4e8be138ed08095ca3e662","evidence":{"kind":"command","detail":"docs/verification/RFC-BPR-0031-home-integration.md: real Settings gestures persist and fan out within one profile, remain isolated across profiles, and survive profile restart."}},{"criterion":"B5","outcome":"pass","revision":"b299818e36887bdf3d4e8be138ed08095ca3e662","evidence":{"kind":"observation","detail":"docs/verification/RFC-BPR-0031-home-integration.md: Home covers and returns to two live reviews while comparison, selection, scroll, epochs, focus, input, and sibling state remain intact."}},{"criterion":"B9","outcome":"pass","revision":"b299818e36887bdf3d4e8be138ed08095ca3e662","evidence":{"kind":"command","detail":"docs/verification/RFC-BPR-0031-home-integration.md: reduced/hidden/inactive and non-minimizing state-change paths cancel or preserve the single timer correctly; inactive sibling has no wakeup and shutdown drains."}},{"criterion":"B10","outcome":"pass","revision":"b299818e36887bdf3d4e8be138ed08095ca3e662","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0031-home-integration.md: three profiles and multiple windows share one ten-strip decode/cache identity, with bounded retained/conversion accounting and no per-tick decode."}},{"criterion":"B11","outcome":"pass","revision":"b299818e36887bdf3d4e8be138ed08095ca3e662","evidence":{"kind":"observation","detail":"docs/verification/RFC-BPR-0031-home-integration.md: 1.0/1.25/2.0 real Home runs sample actual accepted frames, preserve input focus/status, verify compact-first lazy behavior, and span a static reduced interval."}}]
---

# BPR-0762: Prove Home, profile preference and shared-cache behavior

## Outcome

Run one focused app course through real gestures, restart, two windows and two profiles; compare timers/decodes/reading state rather than repeatedly rebuilding everything.

## Acceptance

- Actual rendered frames differ at expected full-motion times; reduced intervals remain static.
- Same-profile preference fan-out and cross-profile isolation pass; one shared decode lifetime is demonstrated.
- Home/input/return-to-review regression checks and hide/deactivate behavior pass.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
