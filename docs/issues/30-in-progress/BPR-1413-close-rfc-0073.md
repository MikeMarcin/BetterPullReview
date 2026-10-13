---
schema: "implement-rfc.card.v1"
id: "BPR-1413"
title: "Verify and close RFC-BPR-0073"
status: "In Progress"
rfcs: ["RFC-BPR-0073"]
depends_on: []
closeout_for: ["RFC-BPR-0073"]
priority: "P1"
metadata: {"milestone_plan":{"id":"smooth-diff-touchpad-panning","path":"docs/rfcs/plans/smooth-diff-touchpad-panning/milestone-plan.json"}}
---

# Smooth diff touchpad panning

Implement all AC01–AC08 of [RFC-BPR-0073](../../rfcs/RFC-BPR-0073-smooth-diff-touchpad-panning.md) as one integrated milestone. Preserve the drift-suppression and discrete-keyboard owner amendments, shared vertical authority and native input geometry.

## Acceptance

- AC01–AC06 and AC08: Direct composed deterministic checks, including 100%, 125%, 200%, zoom, selection/hit testing, old input behavior and large-file budgets.
- AC07: Actual owner Accept/Reject for horizontal panning on the exact self-contained frozen build in a fresh short attended session.
- Complete semantic documentation, explicit evidence and RFC upkeep; retain human pending until the actual verdict.

## Continuation

RFC base: `268b87049592ca4643e5303ab77114e116e7b5f3`.

The [checkpoint](../_checkpoints/smooth-diff-touchpad-panning/README.md) preserves mandate, strategy and source identities. No product acceptance yet.

## Closeout checklist

| Criterion | Check | Result | Commit |
|---|---|---|---|
| **AC01:** In rendered split and unified comparisons, successive vertical pixel deltas smaller than a line visibly move text within the line. A gesture ending between line boundaries holds that position; reversing accepted motion retraces it without cumulative drift. | Pending | Pending | Pending |
| **AC02:** From a mid-file position near a former line-snap boundary, repeated horizontal touchpad events with approximately 20 pixels of horizontal travel and at most 1 pixel of incidental vertical travel pan horizontally without vertical displacement. Reversing horizontal direction, reaching its edge, and ending the gesture release no delayed vertical step. | Pending | Pending | Pending |
| **AC03:** Small vertical-only input remains effective, a sustained comparable-axis diagonal gesture moves both available axes, and deliberate reorientation works within a gesture. A new gesture, forwarded event or comparison/presentation change does not inherit stale suppression. | Pending | Pending | Pending |
| **AC04:** At fractional origins, the rendered ribbons still meet the exact source boundaries they name, and gutters, marks and conversation anchors follow the text. Clicking or selecting a partially clipped row resolves its actual source identity. Verify at 100%, 125% and 200% scale and after a code-zoom change using the applied geometry. | Pending | Pending | Pending |
| **AC05:** Insertion pinning, unequal-replacement motion, Home/End, scroll-past-end, pane handoff, wheel fallback, Shift-wheel, modified zoom and semantic reveals retain their defined behavior. Movement does not create dead scrollbar travel or transfer vertical authority to a pane. | Pending | Pending | Pending |
| **AC06:** A large-file composed scrolling course retains existing input budgets with no semantic recomputation, unchanged-axis rebuild or whole-file text scan per event. Evidence observes the real pane origins and rendered geometry as well as controller position. | Pending | Pending | Pending |
| **AC07:** After deterministic checks, the owner accepts horizontal touchpad panning on a self-contained frozen build in a fresh short attended-review session: “Does horizontal panning stay at the intended vertical reading position despite minor finger wobble?” Use Accept/Reject with a change note; no desktop driving or capture by the implementer. | Pending | Pending | Pending |
| **AC08:** From a fractional origin, Up and Down still navigate one source line per step and retain Shift selection behavior. Whenever keyboard input moves the viewport, the receiving source presentation ends on a line boundary, including repeated arrows, page keys and keyboard reveals; an in-view cursor/selection move leaves the fractional origin unchanged. Subsequent touchpad movement starts at that aligned origin. Verify split and unified views, including an unequal replacement in split view. | Pending | Pending | Pending |
