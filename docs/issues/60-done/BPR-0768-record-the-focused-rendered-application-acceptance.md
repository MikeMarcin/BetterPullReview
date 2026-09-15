---
schema: "implement-rfc.card.v1"
id: "BPR-0768"
title: "Record the focused rendered application acceptance"
status: "Done"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0762", "BPR-0765"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"verification-closeout","acceptance":["B5","B6","B7","B11","B12"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"Every explicit new UI/visual criterion has its own exact answer and revision binding","outcome":"pass","revision":"source-62354ed6-dev-ae238b6379bd2276e7ca1ab2672ba16827447980011ebb57db9fa533011ffed9","evidence":{"kind":"artifact","detail":"Session ef04771118304d90b3b73259e0604e58 records four human accepts at sequences 13-16 for the exact appearance, Full/Reduced motion, placement/controls, and System-motion fingerprints; sequence 17 handed back with pending []."}},{"criterion":"Contact-sheet/raw-viewer output is not substituted for actual application motion","outcome":"pass","revision":"source-62354ed6-dev-ae238b6379bd2276e7ca1ab2672ba16827447980011ebb57db9fa533011ffed9","evidence":{"kind":"observation","detail":"Each accepted candidate required the exact bound Cocoa/QMacStyle executable on a real display; the motion decisions cover actual-speed Full/Reduced and System-setting transitions."}},{"criterion":"Partial replies are preserved and independent automation can proceed; no fabricated verdict","outcome":"pass","revision":"source-62354ed6-dev-ae238b6379bd2276e7ca1ab2672ba16827447980011ebb57db9fa533011ffed9","evidence":{"kind":"artifact","detail":"The corrected external session contains four independent human decisions and no pending candidate; the historical bundled candidate remained unanswered and was not reused."}}]
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
