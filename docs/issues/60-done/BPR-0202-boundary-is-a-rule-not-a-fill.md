---
id: BPR-0202
title: Draw the change-group boundary as a rule on the edge, not a fill on a line
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0198]
---

# Draw the change-group boundary as a rule on the edge, not a fill on a line

Rejected by the product owner on 2026-08-07, in two observations that turned out
to be one defect seen from two sides.

> Sorry change group marker is a highlight of a line unrelated to the change
> group? That is way way confusing

> I will also say that blue typically reads as selection, if that prefix_27
> wasn't selected i didnt see a selection.

## What was wrong

**The placement.** A boundary is a position *between* two lines. The old code
marked it by filling the unchanged line above the group edge to edge — the same
gesture the added, removed and moved bands use, which on this surface means
"this line changed". Spending it on a line that had not changed inverted the
vocabulary. [[BPR-0198]] chose that placement to avoid taking a changed line's
colour away, and recorded the cost in its own comment ("an unchanged line
acquires a role it did not have… 'only option available' is not 'owner
approved'"). The owner supplied the missing approval, in the negative.

**The colour.** The fill was `diff_hunk_header_background`, pale blue. Blue is
the universal selection signal. So the boundary looked selected and the
selection — a desaturated wash at ΔE 5.1 — looked like nothing. In the previous
attended batch the owner identified the boundary marking as the selected line.
They were reading the vocabulary correctly; the vocabulary was wrong.

Both halves matter. Moving the marking to the edge but keeping it blue would
have preserved the second defect.

## What replaced it

A rule, `BoundaryRuleWidth` device-independent pixels, drawn on the edge above
each group's first line, in a new `diff_group_boundary_rule` role that is a
neutral grey in every shipped profile. Blue is now reserved for selection.

The role is new because none of the existing 45 meant "structural furniture on
the code canvas", and reusing one that did not would have left the next reader
believing the palette had a vocabulary it lacks. It is registered as a
`contrast_kind::indicator` against `code_background` rather than a text pair,
which is the honest obligation for a drawn mark:

| profile | rule | vs code background |
|---|---|---|
| Light | `#8C8C8C` | 3.36:1 |
| Dark | `#808080` | 4.79:1 |
| High contrast light | `#000000` | 21.0:1 |
| High contrast dark | `#FFFFFF` | 21.0:1 |

All neutral — chroma zero — so none can be mistaken for a selection.

Note the metric changes with the mechanism, and that is the point rather than a
convenience. [[BPR-0199]] argues a luminance ratio is the wrong instrument for a
background *wash*, because it is blind to the hue doing the work. A drawn rule
is a mark, not a wash, so the indicator ratio is exactly right for it. Moving
the boundary from a fill to a rule moved it into the class of thing WCAG's
non-text minimum was written for.

## Painting

`source_pane::paintEvent` walks only blocks the viewport can show and enters the
sorted boundary list by bisection, so the work follows the visible line count
and not the diff's size. The list is computed when a result is published, not
per repaint.

`GroupBoundariesPainted` now counts rules where they are *drawn* rather than
where they are requested. A queued extra selection is the whole of a band's
effect, so counting intent is sound there; a rule is only real once the viewport
has painted it, and counting the request would let a rule that never reached the
screen read as painted.

## Verification

`qt_diff_workbench` at scales 100, 125 and 200 asserts, from grabbed pixels:

- a run in the boundary colour spanning the pane at the edge above a visible
  group's first line, and
- that the unchanged line above that group carries no band of its own.

Both were shown non-vacuous by breaking them separately, each with a clean
build:

- moving the rule back to `First.Left - 1` fails the first assertion at all
  three scales;
- reintroducing the old fill fails the second.

The first attempt at the second break failed the bounded-work assertion instead,
because the injected loop was not viewport-bounded like the original. Recorded
because the fix was to make the break *faithful* — an unfaithful break tests the
wrong thing and its failure proves nothing about the assertion under test.

The contrast suite's counting assertions were scoped rather than renumbered
where their intent was coverage: syntax-over-background counts by
`contrast_kind::text`, state indicators by their `application_surface`
background. Only the total pair count moved, 209 to 210.
