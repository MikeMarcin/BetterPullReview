---
schema: "implement-rfc.card.v1"
id: "BPR-0087"
title: "Strengthen composed diff-replay input and selection assertions"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0082"]
---

# Strengthen composed diff-replay input and selection assertions

The BPR-0083 independent critical review found two places where the composed
public-path replay records a fact without constraining it. Neither is a defect,
and both behaviors are separately proven at component level, so this is
evidence hardening rather than a correctness repair.

First, the wheel steps in `capture_diff_interaction_evidence` record
`leftInputPosition` after the wheel event but never assert it against
`inputOrigin`, and the later receiver-equivalence steps compare the right pane
and divider to that same recorded value. If no receiver handled the wheel, all
three would agree trivially at the origin and the steps would still pass.
Require `leftInputPosition > inputOrigin`, and require the receiver-equivalence
pre-states to differ from the origin, so the composed replay proves the wheel
moved something rather than only that the three receivers agree.

Second, the composed ribbon click targets the fixture's first group, which the
workbench auto-selects at publish, so `selected_group_id` is identical across
all eighteen records. The existing assertion proves delivery and agreement but
would still pass if selection replacement were broken. Click a ribbon whose
group differs from the current selection and require the selection to be
replaced by that group.

Both changes alter the recorded trace, so the eighteen-record sequence and its
full-window images must be regenerated and re-inspected, and the minimum
rendered evidence sequence must still be satisfied.

## Accepted without change

Two further review observations are recorded as accepted rather than actioned.
PNG content is constrained by direct inspection rather than a machine gate; the
capture only requires the grab to save. Hardening that would need a pixel probe
sampling ribbon edges against pane line rectangles, which is disproportionate
to the risk while inspection is part of the closeout. Separately, the reported
applied origin is derived from the scrollbar fraction rather than observed from
block geometry, which is exact for the uniform-height no-wrap fixtures in use;
any residual seam-level divergence belongs to RFC-BPR-0005 presentation polish.

## Required direct evidence

- `application_diff_workbench_public_path` passes at HEAD with the
  strengthened assertions, using one offscreen application process and
  `CTEST_PARALLEL_LEVEL=1`.
- The regenerated trace and images are re-inspected and still satisfy the
  minimum rendered evidence sequence.
- Each strengthened assertion is shown to fail when the behavior it constrains
  is neutered, so neither is vacuous.

## Direct verification

The wheel steps now require `leftInputPosition > inputOrigin` before the
receiver-equivalence comparisons run, and the right-pane and divider steps each
require their own post-wheel position to exceed the origin as well as to match
the left pane within 1e-6. The divider step also confirms the reset to the
origin actually took effect before its wheel event.

`select_link` is private, and widening it for a test would have been the wrong
trade, so the selection change is driven the way a user drives it. The replay
now finds a visible link whose group differs from the replacement's, clicks it
through the same public link map, and requires the selection to become that
other group and to differ from the replacement's group. The replacement click
then has to replace it. An extra capture between the two clicks makes the
recorded artifact show the change rather than only the assertions proving it,
so the sequence is nineteen records and nineteen images rather than eighteen.

Direct inspection of the two new frames confirms the behavior. Step 15 shows
the other group selected with the detail pane reading `Change 2 of 3` and the
replacement ribbon unselected; step 16 shows the 4:7 replacement ribbon filled
and attached across `old-a..old-d` and `new-a..new-g`, the left `old-a` row
highlighted, the detail pane reading `Change 1 of 3`, and `Previous change`
correctly disabled at the first change.

Neither assertion is vacuous. Making `send_diff_evidence_wheel` return without
sending fails the run, and pointing the second click at the first ribbon's
point so the selection never changes also fails it; both restore green.

`application_diff_workbench_public_path` passes at 12.87 seconds with
`QT_QPA_PLATFORM=offscreen` and `CTEST_PARALLEL_LEVEL=1`. Exactly one
application process ran and exited with no leftover process, and the evidence
directory holds nineteen records and nineteen images. The adjacent
`application_diff_workbench_shutdown_public_path` and the tests-disabled
Release `application_composition_contract` pass alongside it, and the 182-file
format gate passes.

## Result

The composed replay's two weak assertions are now load-bearing, and the
recorded artifact demonstrates a click replacing a semantic selection instead
of agreeing with one that was already current. BPR-0087 is complete.
