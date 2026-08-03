---
id: BPR-0087
title: Strengthen composed diff-replay input and selection assertions
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0083]
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
