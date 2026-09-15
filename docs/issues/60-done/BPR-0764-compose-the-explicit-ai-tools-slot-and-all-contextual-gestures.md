---
schema: "implement-rfc.card.v1"
id: "BPR-0764"
title: "Compose the explicit AI tools slot and all contextual gestures"
status: "Done"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0761", "BPR-0763"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"context-integration","acceptance":["B6","B7","B11"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Actual app UI routes every contextual clip through accepted operations","outcome":"pass","revision":"ad0bb67d","evidence":{"kind":"observation","detail":"The workbench composes one explicit AI tools and Findings selector and the existing application fanout routes compare_glance, consider, comment_ready, attention, acknowledge, and blocked after their accepted presentations."}},{"criterion":"No avatar/overlay/autonomous drawer opening","outcome":"pass","revision":"ad0bb67d","evidence":{"kind":"command","detail":"qt_workbench_window_scale_100/125/200 passed; the selector remained collapsed when its drawer was collapsed and no Dif widget appeared outside Home or the selected AI slot."}},{"criterion":"Exactly one eligible slot and static AI baseline","outcome":"pass","revision":"ad0bb67d","evidence":{"kind":"command","detail":"The multi-scale workbench course observed a single lazily decoded widget move between Home and the 64 px AI slot; reduced motion stayed on rest without a timer, narrow layout yielded the slot, and Home won placement."}}]
---

# BPR-0764: Compose the explicit AI tools slot and all contextual gestures

## Outcome

Reuse the workbench sprite widget between Home and the visible existing AI tools area. Keep static rest while working; reveal/select Findings drives one comparison glance. Preserve every provider/author label.

## Acceptance

- Actual app UI has routes to compare_glance, consider, comment_ready, attention, acknowledge and blocked.
- No card/message avatar or code/shared-comment overlay; drawer/tab does not open itself.
- Exactly one eligible slot per window; no continuous idle in the AI area.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
