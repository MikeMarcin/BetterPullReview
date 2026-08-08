---
id: BPR-0205
title: A one-line selection cannot show whether the change tint survives it
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0196]
---

# A one-line selection cannot show whether the change tint survives it

Product owner, 2026-08-07, on the `selection_shade` batch:

> In order to tell if the red removed tint is readable through the selection I
> need a selection of more than just 1 line, it needs to select a non-removed
> line as well to contrast

This is a defect in how the question was asked, and it invalidates one of the
three acceptance criteria the batch carried.

The criterion was "the red removed tint is still readable through the
selection". Every capture showed the selection on a single line, which happened
to be a removed line. With nothing to compare against, a reviewer sees one
washed row and cannot tell whether the red underneath survived, was replaced,
or was never there. The comparison the criterion asks for was not in the frame.

Candidate C was accepted on the first criterion — "you can tell which line is
selected" — which the evidence *did* support. The second criterion is
unanswered, not satisfied.

## Two things this needs

**Evidence.** A capture where the selection spans a removed line and an
unchanged line together, so the two backgrounds appear under the same wash and
can be compared directly. That is the only arrangement in which the question is
answerable.

**A behaviour, which may not exist yet.** [[BPR-0194]] already records that
p4merge marks a multi-line span and that the owner called that the reference
behaviour, with the note that "nothing owns that yet". Confirm whether the
workbench can select a range at all before building a capture around it. If it
cannot, that is the card this one becomes, and the evidence gap is downstream
of it rather than beside it.

## Also unresolved: the reviewer still had to assume

The same verdict opens:

> I assume teardown pipeline is selected

The batch text named the selected line explicitly and placed it directly below
the new boundary rule, and the reviewer still hedged. Naming it in prose is not
enough — the image has to make it self-evident. Future selection captures should
mark the selected row outside the pane, or show the identical frame with the
selection on and off so the difference is the answer rather than something to be
located.

Note this hedge is weaker evidence than the same phrase in the previous batch,
where the marking genuinely was ambiguous. Here the owner went on to say C was
"the only one I can easily see without eye strain", so the shade was legible;
what remained uncertain was which row the question was about.

## Fixed, 2026-08-07 — the model already said it, the painter ignored it

`semantic_selection_kind::group` has existed all along, and a `change_group`
carries its own `Left` and `Right` line spans. Clicking a ribbon has always
produced a group selection. The painter simply washed
`CurrentDetail->LeftLine` whatever the kind, so a group selection rendered as a
line selection and nothing downstream could tell.

`diff_workbench::selected_lines` now answers with the group's span when the
selection is a group and the single line when it is a line. A group the
alignment no longer holds yields nothing rather than falling back to the
preferred line — a wash drawn somewhere other than where the model says the
selection is would be worse than no wash.

`append_selection_band` takes a span and clips it to the paintable window, so
selecting a group the size of the file costs the viewport and not the file.

### It answers the criterion that could not be answered

Rendered with a ribbon selected, in both schemes: on the left the removed lines
read lavender — the blue wash over the red band — and on the right the added
lines read teal, blue over green. Both sit directly above unwashed, unchanged
code. The comparison the owner asked for is now in a single frame, which is
what makes "is the change tint readable through the selection" a question the
evidence can settle.

### Verification

`qt_diff_workbench` selects through the ribbon rather than by setting a field,
because the claim is that a reviewer can reach this and not that the painter
reads a struct member. It then counts, from the pane's own extra selections,
the wash-coloured full-width bands inside the group's span.

**Matched by colour, not merely by being full width.** The added and removed
bands are also full-width selections over those same lines, so a count that
ignored colour tallied them and passed with the wash collapsed to one row —
which the first draft of this test did. Corrected, collapsing the span fails it
at all three scales with a clean build.

## Superseded — required direct evidence

- A capture with a multi-line selection crossing a change boundary, and the
  removed tint measured under the wash on the removed row and absent on the
  unchanged row, from pixels.
- If range selection has to be built, its own assertions, shown non-vacuous.
- The selected row is identifiable from the image alone, without reading the
  caption.
