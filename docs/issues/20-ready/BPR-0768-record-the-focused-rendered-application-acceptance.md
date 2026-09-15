---
schema: "implement-rfc.card.v1"
id: "BPR-0768"
title: "Record the focused rendered application acceptance"
status: "Ready"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0762", "BPR-0765"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"verification-closeout","acceptance":["B5","B6","B7","B11","B12"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Every explicit new UI/visual criterion has its own exact answer and revision binding","outcome":"pending","revision":"source-62354ed6-dev-ae238b6379bd2276e7ca1ab2672ba16827447980011ebb57db9fa533011ffed9","evidence":{"kind":"artifact","detail":"Fresh session ef04771118304d90b3b73259e0604e58 has four ready protected candidates at sequence 12: native appearance 8f8d7b7b923e068d772149f60e54215f6d870b0052f1ecbe6e2330c7811b715e; placement/controls 45b03f0ff28503f647f40a4eb5d718b9f26f8811b154a7ec137a69e423f876ae; Full/Reduced motion 94235df2567d102f6951802e619123f67a8fc90f0d23805a93a22cd86ae62c2d; System motion b8cc61ad6e02d9126d15df688f57217ecf67b9384dc5f4c79b6953d90c98969b."}},{"criterion":"Contact-sheet/raw-viewer output is not substituted for actual application motion","outcome":"pass","revision":"0df6f24c","evidence":{"kind":"observation","detail":"Automated captures and the tests-disabled native render are explicitly supporting evidence; each protected motion/appearance candidate requires the bound Cocoa/QMacStyle fixture app at actual speed."}},{"criterion":"Partial replies are preserved and independent automation can proceed; no fabricated verdict","outcome":"pass","revision":"fe12d5a9","evidence":{"kind":"artifact","detail":"The first bundled candidate was released unanswered. Corrected external state is ready at sequence 12 with no decisions; BPR-0768 remains parked for human-review and automated BPR-0766/0767 evidence remains independent."}}]
---

# BPR-0768: Record the focused rendered application acceptance

## Outcome

Prepare one attended real-app session for integration questions, reusing unchanged accepted art evidence. Show actual-speed/size placement, full/reduced modes, nonintrusive controls and representative transitions.

## Acceptance

- Every explicit new UI/visual criterion has its own exact answer and revision binding.
- Contact-sheet/raw-viewer output is not substituted for actual application motion.
- Partial replies are preserved and independent automation can proceed; no fabricated verdict.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.
