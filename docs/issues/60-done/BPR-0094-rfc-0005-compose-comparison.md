---
schema: "implement-rfc.card.v1"
id: "BPR-0094"
title: "Compose the RFC-BPR-0004 comparison surface into the workbench"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0093"]
---

# Compose the RFC-BPR-0004 comparison surface into the workbench

Compose the existing natural-layout old/new panes, the single shared comparison controller and vertical scrollbar, and the custom-painted `DiffLinkMap` into the workbench shell.

Introduce no second mapping, no presentation-side regrouping, no follower-scroll coupling, and no synthetic alignment rows. The two top documents contain no invented source rows, and exactly one shared vertical scrollbar exists.

## Implementation

The comparison placeholder is replaced by the existing RFC-BPR-0004
`diff_workbench`, composed as-is. No wrapper authority was introduced: the
window creates the surface, places it in the centre, and exposes it. Scroll
authority, mapping, grouping, and painting all remain where RFC-BPR-0004 put
them, so no second mapping, presentation-side regrouping, follower-scroll
coupling, or synthetic row source was added.

## Direct verification

- `qt_workbench_window_scale_100`, `_125`, and `_200` pass offscreen and
  serial, with both architecture boundary suites. The full Qt adapter suite
  passes 24/24. The format gate and strict `clang-tidy` with
  `--warnings-as-errors` pass.
- Non-vacuity, one-shared-scrollbar rule: forcing the left pane's vertical
  scrollbar policy to `ScrollBarAlwaysOn` fails with "the comparison region
  shows exactly one vertical scrollbar". Restored to green.
- Non-vacuity, no-synthetic-rows rule: this one needed isolating. Injecting a
  presentation-side row, the exact defect the RFC forbids, first tripped a
  scrollbar assertion because the extra row changed scrollbar visibility and
  the harness exits on first failure. That red proved the composition is
  defended but did not prove which rule caught it. Temporarily neutralising the
  two earlier scrollbar assertions let the run reach the document checks, which
  failed with "each top document is exactly its own source text". The row is
  therefore caught by exact text equality, independent of the scrollbar
  assertions. Both the injected defect and the neutralised assertions were then
  restored, and the file was verified to contain no `TEMP-BREAK` or `true ||`
  residue.

The synthetic-row guard is genuinely two assertions: exact document text
equality, and document row counts against the alignment's own left and right
line counts. Either alone would catch an invented row; together they also catch
substituted content of the same length.

## Process note

The delivering worker stopped mid-verification again, this time having
restored its break first, so the tree arrived green rather than silently
broken. Its own report identified the masking problem it had hit and what it
intended to do about it, which is what let this pass resume the isolation
directly rather than rediscover it.

## Retained risks

- The surface is composed but nothing yet drives it from the real application;
  composition-root wiring is a later card.
- Ribbon-to-group agreement is asserted through the model and geometry, not
  from an inspected raster. Rendered evidence belongs to BPR-0097 and BPR-0100.

## Result

The workbench centre now hosts the real RFC-BPR-0004 comparison surface with
exactly one shared vertical scrollbar, both panes without vertical scroll
authority, and no invented rows in either document. BPR-0094 is complete.
