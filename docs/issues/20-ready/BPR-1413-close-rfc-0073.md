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
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"60aa863613259ce3e4cad77f5517582187353492","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md: AC01 direct composed evidence; m1-verification.json exact command results"}},{"criterion":"AC02","outcome":"pass","revision":"60aa863613259ce3e4cad77f5517582187353492","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md: AC02 direct composed evidence; m1-verification.json exact command results"}},{"criterion":"AC03","outcome":"pass","revision":"60aa863613259ce3e4cad77f5517582187353492","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md: AC03 direct composed evidence; m1-verification.json exact command results"}},{"criterion":"AC04","outcome":"pass","revision":"60aa863613259ce3e4cad77f5517582187353492","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md: AC04 direct composed evidence; m1-verification.json exact command results"}},{"criterion":"AC05","outcome":"pass","revision":"60aa863613259ce3e4cad77f5517582187353492","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md: AC05 direct composed evidence; m1-verification.json exact command results"}},{"criterion":"AC06","outcome":"pass","revision":"60aa863613259ce3e4cad77f5517582187353492","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md: AC06 direct composed evidence; m1-verification.json exact command results"}},{"criterion":"AC07","outcome":"pending","revision":"60aa863613259ce3e4cad77f5517582187353492","evidence":{"kind":"artifact","detail":"docs/issues/_checkpoints/smooth-diff-touchpad-panning/README.md: fresh frozen-build attended session; actual owner verdict pending"}},{"criterion":"AC08","outcome":"pass","revision":"60aa863613259ce3e4cad77f5517582187353492","evidence":{"kind":"artifact","detail":"docs/verification/RFC-BPR-0073.md: AC08 direct composed evidence; m1-verification.json exact command results"}}]
---

# Smooth diff touchpad panning

Implement all AC01–AC08 of [RFC-BPR-0073](../../rfcs/RFC-BPR-0073-smooth-diff-touchpad-panning.md) as one integrated milestone. Preserve the drift-suppression and discrete-keyboard owner amendments, shared vertical authority and native input geometry.

## Acceptance

- AC01–AC06 and AC08: Direct composed deterministic checks, including 100%, 125%, 200%, zoom, selection/hit testing, old input behavior and large-file budgets.
- AC07: Actual owner Accept/Reject for horizontal panning on the exact self-contained frozen build in a fresh short attended session.
- Complete semantic documentation, explicit evidence and RFC upkeep; retain human pending until the actual verdict.

## Continuation

RFC base: `268b87049592ca4643e5303ab77114e116e7b5f3`.

The [checkpoint](../_checkpoints/smooth-diff-touchpad-panning/README.md) preserves mandate, strategy and source identities. Implementation candidate `60aa863613259ce3e4cad77f5517582187353492` passes AC01–AC06 and AC08; the required owner verdict remains pending. The checkpoint names the fresh frozen-build review session and exact continuation command.

## Closeout checklist

| Criterion | Check | Result | Commit |
|---|---|---|---|
| **AC01:** In rendered split and unified comparisons, successive vertical pixel deltas smaller than a line visibly move text within the line. A gesture ending between line boundaries holds that position; reversing accepted motion retraces it without cumulative drift. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `60aa86361` |
| **AC02:** From a mid-file position near a former line-snap boundary, repeated horizontal touchpad events with approximately 20 pixels of horizontal travel and at most 1 pixel of incidental vertical travel pan horizontally without vertical displacement. Reversing horizontal direction, reaching its edge, and ending the gesture release no delayed vertical step. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `60aa86361` |
| **AC03:** Small vertical-only input remains effective, a sustained comparable-axis diagonal gesture moves both available axes, and deliberate reorientation works within a gesture. A new gesture, forwarded event or comparison/presentation change does not inherit stale suppression. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `60aa86361` |
| **AC04:** At fractional origins, the rendered ribbons still meet the exact source boundaries they name, and gutters, marks and conversation anchors follow the text. Clicking or selecting a partially clipped row resolves its actual source identity. Verify at 100%, 125% and 200% scale and after a code-zoom change using the applied geometry. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `60aa86361` |
| **AC05:** Insertion pinning, unequal-replacement motion, Home/End, scroll-past-end, pane handoff, wheel fallback, Shift-wheel, modified zoom and semantic reveals retain their defined behavior. Movement does not create dead scrollbar travel or transfer vertical authority to a pane. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `60aa86361` |
| **AC06:** A large-file composed scrolling course retains existing input budgets with no semantic recomputation, unchanged-axis rebuild or whole-file text scan per event. Evidence observes the real pane origins and rendered geometry as well as controller position. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `60aa86361` |
| **AC07:** After deterministic checks, the owner accepts horizontal touchpad panning on a self-contained frozen build in a fresh short attended-review session: “Does horizontal panning stay at the intended vertical reading position despite minor finger wobble?” Use Accept/Reject with a change note; no desktop driving or capture by the implementer. | [Fresh frozen-build session](../_checkpoints/smooth-diff-touchpad-panning/README.md) | Pending owner | `60aa86361` |
| **AC08:** From a fractional origin, Up and Down still navigate one source line per step and retain Shift selection behavior. Whenever keyboard input moves the viewport, the receiving source presentation ends on a line boundary, including repeated arrows, page keys and keyboard reveals; an in-view cursor/selection move leaves the fractional origin unchanged. Subsequent touchpad movement starts at that aligned origin. Verify split and unified views, including an unequal replacement in split view. | [Direct evidence](../../verification/RFC-BPR-0073.md#criterion-evidence) | Pass | `60aa86361` |
