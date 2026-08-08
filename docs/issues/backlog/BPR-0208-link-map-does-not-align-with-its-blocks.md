---
id: BPR-0208
title: The gutter link map does not line up with the blocks it connects
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0192]
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
