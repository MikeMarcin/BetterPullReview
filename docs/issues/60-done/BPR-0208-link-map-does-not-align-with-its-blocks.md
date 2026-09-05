---
schema: "implement-rfc.card.v1"
id: "BPR-0208"
title: "The gutter link map does not line up with the blocks it connects"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0192"]
---

# The gutter link map does not line up with the blocks it connects

Product owner, 2026-08-07, accepting the light scheme but noting:

> The gutter swoosh (visible clearly in the whole window) doesn't line up with
> the text blocks

The ribbons drawn between the two panes are meant to join a run of lines on the
left to the run it corresponds to on the right. In the captured window they
meet the panes at the wrong height, so a ribbon appears to start above or below
the block it belongs to.

Evidence: `docs/verification/reviews/diff-scheme-acceptance-v001/`, subject
`light-after-refit`, the whole-window image.

## Why this matters more than it looks

The ribbon is the only thing on the surface that says *these two runs are the
same code*. A diff that draws the connection in the wrong place is not
decorative noise — it asserts a correspondence that is false, and a reviewer
who trusts it reads the wrong pair. That is worse than drawing nothing.

It is also the one element whose correctness cannot be checked from a
line-by-line reading of the panes: the panes are individually right, and only
the geometry between them is wrong.

## Where to look

`diff_link_map` computes ribbon endpoints from the panes' scroll frames. The
likely candidates, in the order worth checking:

1. The endpoint is derived from a line number times a line height rather than
   from the pane's own block geometry, so it drifts wherever a block is not
   exactly one line tall or the viewport is scrolled to a fractional offset.
2. The map's own vertical origin is not the panes' origin — a header, ribbon
   strip or margin offsets one and not the other, which would show as a
   constant displacement rather than a drifting one.
3. The frame the map draws from is a repaint behind the frame the panes are
   showing, which would misalign only while scrolling and settle when idle.

Those three have different signatures. A constant offset points at 2, an error
that grows down the file points at 1, and one that appears only in motion
points at 3. **Measure which before changing anything** — the capture on file is
a single still, so it cannot distinguish them.

## Measured, 2026-08-07 — two causes, one fixed

Instrumenting the layout and then the endpoints found both, and they are
independent.

### Cause A, fixed: the map's band was not the panes' band

```
map_top=0  left_viewport_top=1  map_h=306  pane_h=286
```

Ribbon endpoints are computed in a pane's *viewport* coordinates and drawn on a
widget that is a **sibling** of the panes. Its top differs by the pane frame,
and its height by whatever a horizontal scrollbar takes from the viewport and
not from the map — twenty pixels here. The map was using its own `height()` for
culling and its own `y` for drawing, so both the window and the offset were
wrong.

`diff_link_map::set_viewport_band` now takes the offset and the extent, and
`diff_workbench::declare_link_map_band` supplies them from the real widgets on
publish and on resize. Declared rather than inferred, because the difference
depends on style and on whether a scrollbar is showing, neither of which the
map can know.

### Cause B, diagnosed and not fixed: the document origin is an estimate

With the band correct, a ribbon still missed its block by **nine pixels** at
line 29 of the fixture:

```
scrollval=14  docmargin=4  leftTop=234.7  cursorTop=225  delta=9
```

`source_pane::set_document_origin` returns
`appliedFraction * aMaximumOrigin` — the fraction between the scrollbar's
minimum and maximum, scaled. **`QPlainTextEdit` scrolls in blocks, and the last
page cannot be scrolled past**, so that fraction is not the fraction of the
document above the viewport. The two agree at the top and diverge as the file
is scrolled, which is exactly the drifting signature this card listed as
candidate 1 — for the origin rather than for the endpoints.

Two attempts at a local fix failed and were reverted:

- `QAbstractTextDocumentLayout::blockBoundingRect(block).top()` returns `0.0`
  for every block in this path, including blocks well down the document. The
  layout is not resolved when the frame is applied, so it cannot supply the
  document y.
- `blockBoundingGeometry(block).translated(contentOffset()).top()` returns
  `0.0` in the same call for the same reason.

The measurement that *is* trustworthy already exists: `pane_line_measurements::
Boundaries`, accumulated block by block from `documentMargin`, is what the
endpoints themselves are built from. But `measured_alignment_axis` keeps only
`Boundaries.front()`, so the array is discarded before the frame path could use
it.

**So the fix is: retain the boundaries on the axis and compute the origin as
`Boundaries[firstVisibleBlock] - contentOffset().y()`.** That makes the origin
and the endpoints share one measurement basis, so they cannot disagree by
construction — rather than agreeing because two independent derivations were
checked against each other.

### Cause B, fixed

`measured_alignment_axis` now retains the per-line boundaries instead of
keeping only their first element, and exposes `left_line_origin` /
`right_line_origin`. `source_pane::visible_origin` reports which block the
viewport starts on and how far above the edge it begins — `firstVisibleBlock`
and `contentOffset` are protected, so the pane is the only place they can be
read. The origin is then

    Boundaries[firstVisibleBlock] - contentOffset().y()

which is the same measurement the ribbon endpoints are built from. The
scrollbar-fraction value is still computed, and kept only as a fallback for the
torn case where the pane cannot name a first block or the axis has no boundary
for it.

That is the point of the shape rather than an implementation detail: origin and
endpoints now share one basis, so they cannot disagree by construction. Before,
they were two independent derivations of the same quantity that happened to
agree at the top of a file.

### The assertion

`qt_diff_workbench` at 100, 125 and 200 per cent asserts that a visible
ribbon's endpoint and its block's top are **the same integer**, both mapped
into the parent's coordinates.

Exact, not approximate, and that is load-bearing. Measured at all three scales
the difference is zero rather than small, and the band error is one pixel on
this platform — so any tolerance wide enough to be "safe" would admit the very
error the test exists to catch. An earlier draft used ±2 and passed with the
band deliberately broken.

Both causes shown non-vacuous separately, each with a clean build:

- zeroing the band offset fails it at all three scales;
- returning the scrollbar estimate instead of the measured origin fails it at
  all three scales.

## Required direct evidence

- For a known segment, the ribbon's endpoint y in the rendered raster matches
  the top of that segment's first block in the pane, sampled from pixels.
- Checked at more than one scroll position and at the far end of a long file,
  so a constant offset and a drifting one cannot both pass.
- Checked at 125% and 200% scale, since the existing workbench courses run
  there and a device-pixel-ratio error would show up nowhere else.
- The assertion fails when the endpoint is displaced by one line.

## Do not

Do not correct this by nudging a constant until the capture looks right. If the
cause is 1 or 3, a constant fixes the one frame that was measured and leaves
the error everywhere else — and the next reader will find a magic number with
no derivation next to a test that passes.
