---
schema: "implement-rfc.card.v1"
id: "BPR-1413"
title: "Verify and close RFC-BPR-0073"
status: "Ready"
rfcs: ["RFC-BPR-0073"]
depends_on: []
closeout_for: ["RFC-BPR-0073"]
priority: "P1"
metadata: {"milestone_plan":{"id":"smooth-diff-touchpad-panning","path":"docs/rfcs/plans/smooth-diff-touchpad-panning/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"a9af17c712f2fc3e7dc4c4718689ff482af8fed7","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md; m3-verification.json: seven affected integration checks pass; unchanged controller evidence reused from M2"}},{"criterion":"AC02","outcome":"pass","revision":"a9af17c712f2fc3e7dc4c4718689ff482af8fed7","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md; m3-verification.json: seven affected integration checks pass; unchanged controller evidence reused from M2"}},{"criterion":"AC03","outcome":"pass","revision":"a9af17c712f2fc3e7dc4c4718689ff482af8fed7","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md; m3-verification.json: seven affected integration checks pass; unchanged controller evidence reused from M2"}},{"criterion":"AC04","outcome":"pass","revision":"a9af17c712f2fc3e7dc4c4718689ff482af8fed7","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md; m3-verification.json: seven affected integration checks pass; unchanged controller evidence reused from M2"}},{"criterion":"AC05","outcome":"pass","revision":"a9af17c712f2fc3e7dc4c4718689ff482af8fed7","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md; m3-verification.json: seven affected integration checks pass; unchanged controller evidence reused from M2"}},{"criterion":"AC06","outcome":"pass","revision":"a9af17c712f2fc3e7dc4c4718689ff482af8fed7","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md; m3-verification.json: seven affected integration checks pass; unchanged controller evidence reused from M2"}},{"criterion":"AC07","outcome":"pass","revision":"59a2fa10c147fd1dc976eb5eef161137da5077fe","evidence":{"kind":"observation","detail":"Owner accepted BPR-1413-AC07-v002 in session c595e5c955c2cd527f9f4a351d3ab175, decision 0a0ba98a-3e76-4cb8-b404-fb0196dbd48c; unchanged axis implementation retained."}},{"criterion":"AC08","outcome":"pending","revision":"a9af17c712f2fc3e7dc4c4718689ff482af8fed7","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md; m3-verification.json: seven affected integration checks pass; unchanged controller evidence reused from M2; v002 owner rejected, separate v003 owner minimum keyboard reveal judgment pending"}}]
---

# Smooth diff touchpad panning

Implement all AC01–AC08 of [RFC-BPR-0073](../../rfcs/RFC-BPR-0073-smooth-diff-touchpad-panning.md) as one integrated milestone. Preserve the drift-suppression and discrete-keyboard owner amendments, shared vertical authority and native input geometry.

## Acceptance

- AC01–AC06 and AC08: Direct composed deterministic checks, including 100%, 125%, 200%, zoom, selection/hit testing, old input behavior and large-file budgets.
- AC07: Owner accepted v002 horizontal panning; retain that verdict and the unchanged adaptive axis implementation.
- AC08 owner correction: Separate Accept/Reject for nearest-edge minimum keyboard reveal; full-row visibility and boundary slack are agent-verified.
- Complete semantic documentation, explicit evidence and RFC upkeep; retain human pending until the actual verdict.

## Continuation

RFC base: `268b87049592ca4643e5303ab77114e116e7b5f3`.

The [checkpoint](../_checkpoints/smooth-diff-touchpad-panning/README.md) preserves mandate, strategy and source identities. Candidate v001 was rejected for excessive keyboard reveal and wrong-axis sensitivity. The owner accepted AC07 on v002 and rejected its one-row-early keyboard reveal. Candidate `a9af17c712f2fc3e7dc4c4718689ff482af8fed7` uses the displayed viewport bounds, passes the strengthened AC08 rendered course and all seven affected integration checks. Only the v003 minimum keyboard reveal judgment remains pending. The checkpoint names the fresh frozen-build review session and exact continuation command.

## Closeout checklist

| Criterion | Check | Result | Commit |
|---|---|---|---|
| **AC01:** In rendered split and unified comparisons, successive vertical pixel deltas smaller than a line visibly move text within the line. A gesture ending between line boundaries holds that position; reversing accepted motion retraces it without cumulative drift. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `a9af17c71` |
| **AC02:** From a mid-file position near a former line-snap boundary, repeated horizontal touchpad events with approximately 20 pixels of horizontal travel and at most 1 pixel of incidental vertical travel pan horizontally without vertical displacement. Reversing horizontal direction, reaching its edge, and ending the gesture release no delayed vertical step. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `a9af17c71` |
| **AC03:** Small vertical-only input remains effective, a sustained comparable-axis diagonal gesture moves both available axes, and deliberate reorientation works within a gesture. A new gesture, forwarded event or comparison/presentation change does not inherit stale suppression. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `a9af17c71` |
| **AC04:** At fractional origins, the rendered ribbons still meet the exact source boundaries they name, and gutters, marks and conversation anchors follow the text. Clicking or selecting a partially clipped row resolves its actual source identity. Verify at 100%, 125% and 200% scale and after a code-zoom change using the applied geometry. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `a9af17c71` |
| **AC05:** Insertion pinning, unequal-replacement motion, Home/End, scroll-past-end, pane handoff, wheel fallback, Shift-wheel, modified zoom and semantic reveals retain their defined behavior. Movement does not create dead scrollbar travel or transfer vertical authority to a pane. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `a9af17c71` |
| **AC06:** A large-file composed scrolling course retains existing input budgets with no semantic recomputation, unchanged-axis rebuild or whole-file text scan per event. Evidence observes the real pane origins and rendered geometry as well as controller position. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `a9af17c71` |
| **AC07:** After deterministic checks, the owner accepts horizontal touchpad panning on a self-contained frozen build in a fresh short attended-review session: “Does horizontal panning stay at the intended vertical reading position despite minor finger wobble?” Use Accept/Reject with a change note; no desktop driving or capture by the implementer. | [Fresh frozen-build session](../_checkpoints/smooth-diff-touchpad-panning/README.md) | Owner accepted v002 | `59a2fa10c` |
| **AC08:** From a fractional origin, Up and Down still navigate one source line per step and retain Shift selection behavior. Whenever keyboard input moves the viewport, the receiving source presentation ends on a line boundary, including repeated arrows, page keys and keyboard reveals; an in-view cursor/selection move leaves the fractional origin unchanged. Subsequent touchpad movement starts at that aligned origin. Verify split and unified views, including an unequal replacement in split view. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Agent pass; owner minimum-reveal judgment pending | `a9af17c71` |
