---
id: BPR-0198
title: Moved code, hunk headers, and intraline evidence against the model
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0196]
---

# Moved code, hunk headers, and intraline evidence against the model

BPR-0196 painted three of the reference's seven elements. This card owns the
rest of the palette's diff roles, and the evidence BPR-0196 left short.

## What this slice did

- **Moved code** carries `diff_moved_background` on both sides.
  `file_alignment::Moves` names a source deletion and a destination insertion
  by segment identity; both are now painted as moved rather than as an
  ordinary removal and an unrelated addition, which is exactly the distinction
  move detection exists to compute.
- **Change-group boundaries** carry `diff_hunk_header_background` and
  `diff_hunk_header_text`.
- **Intraline emphasis is compared against the model**, on a fixture whose
  changed line carries a two-byte character ahead of the edit.
- **The alignment scan is bounded**, which it was not.

### Why a hunk header is not a row

The panes show whole documents side by side, not a unified hunk stream. A
header row would make pane line N stop being document line N, and every mapping
built on that equality — intraline emphasis, token spans, selection, the
measured axis — would shift by the number of headers above it. So the
delimiter has to be a marking on a line that already exists.

It goes on the **unchanged line immediately above** the group, which is
otherwise deliberately untinted, so the marking costs no change signal. Putting
it on the group's own first line would take an added or removed line's colour
away to say something the band already implies.

**This is a visible product choice, and it is mine rather than the owner's.**
An unchanged line acquires a role it did not have. It is the only placement the
mechanism allows without custom viewport painting — which RFC-BPR-0005's
measured-axis evidence rests on not happening — but "the only option available"
is not "approved", and BPR-0194 is the standing reminder of that difference. It
belongs in the next attended batch alongside the selection treatment.

### The scan BPR-0196 left unbounded

BPR-0196 bounded the number of bands the painter *emits* and left the loop that
chooses them running over every alignment segment in the file, on every
repaint. The bounded-selection assertion could not see it: the emitted count
was correct and the work to produce it was proportional to the length of the
comparison.

That is the third time this session the same shape has appeared — a count that
is bounded for the right reason while the search behind it is not — after the
extra-selection list in BPR-0196 and the token-span walk in BPR-0185. Segments
are ordered by line, so the visible run is now found by search, and
`AlignmentSegmentsVisited` is asserted bounded at the far end of a file.

Measuring at the far end is load-bearing: at line zero the visible run begins
where the segment list begins, and a scanning painter is indistinguishable
from a searching one.

## A test helper that hid a correct implementation

The first `band_color` helper returned the **last** full-width selection on a
line. Three different things are full-width — the diff band, the group
boundary, and the reviewer's selection — emitted in that order because that is
the order they must composite in. So the helper reported the selection
highlight, and a correctly painted moved band read as `#308cc6` instead of
`#fff1d6`.

The failure looked like a product defect and was a measurement defect. Worth
recording because the fix is not obvious from the symptom: the helper now reads
the first full-width band and says why.

## Non-vacuity

Each break compiled; the error count was checked before the failure was
believed.

| Break | Failure |
|---|---|
| Use the intraline byte offset as a character index | `the painted character range is the range the core computed` |
| Ignore the move table | `a comparison the core reports moves for paints moved bands` |
| Mark the group's own first line instead of the line above | `the line above a change group carries the hunk header background` |
| Scan every alignment segment instead of searching | `a file ten times longer visits no more than a constant factor more alignment segments` |

The fourth break **passed** against the suite as it stood when it was first
run. That is what the new `AlignmentSegmentsVisited` assertion was written for,
and it is the reason this card touched the bound at all.

## Required direct evidence

- A moved block carries `diff_moved_background` on both sides, and is
  distinguishable from a deletion paired with an unrelated addition. **Met**:
  the source segment is asserted to be `left_only` — which is what a surface
  reading no moves would paint as an ordinary removal — and the palette's moved
  and removed colours are asserted distinct before the comparison is made.
- Painted intraline ranges are compared against `computed_file_diff::Intraline`
  resolved through `Pairs`, on a fixture containing at least one multi-byte
  character ahead of an edit. **Met**, and the test asserts that the character
  index and the byte offset actually differ on that line, so a fixture that
  quietly became ASCII would fail rather than pass vacuously. The expectation
  is computed from the model and the document bytes, never read back from the
  widget.
- Hunk headers are painted where the model places change-group boundaries.
  **Met**, with the placement caveat above.
- The bounded-painting assertion from BPR-0196 still holds with the new roles
  applied. **Met, and strengthened**: the alignment scan is now bounded too,
  which it was not when BPR-0196 recorded that assertion.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered. **Met**, tabulated above.

## What this card does NOT deliver

- **No move affordance.** A moved block is coloured; nothing lets a reviewer
  jump between a move's two ends, and the pairing is not otherwise expressed.
- **No header text.** `diff_hunk_header_text` is applied as a foreground on the
  boundary line, but the line still shows its own source text. There is no
  `@@ -1,7 +1,9 @@` summary anywhere, and there cannot be one without a row.
- **Six of seven reference elements**, up from three. The remaining gap is the
  selection treatment BPR-0194 owns.
