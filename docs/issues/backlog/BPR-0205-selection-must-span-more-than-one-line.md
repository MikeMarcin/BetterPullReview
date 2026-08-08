---
id: BPR-0205
title: A one-line selection cannot show whether the change tint survives it
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0194]
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

## Required direct evidence

- A capture with a multi-line selection crossing a change boundary, and the
  removed tint measured under the wash on the removed row and absent on the
  unchanged row, from pixels.
- If range selection has to be built, its own assertions, shown non-vacuous.
- The selected row is identifiable from the image alone, without reading the
  caption.
