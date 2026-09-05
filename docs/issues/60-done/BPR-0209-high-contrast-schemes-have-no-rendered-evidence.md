---
schema: "implement-rfc.card.v1"
id: "BPR-0209"
title: "The two high-contrast profiles have never been seen"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0201"]
---

# The two high-contrast profiles have never been seen

Light and dark are both accepted at attended review. `high_contrast_light` and
`high_contrast_dark` ship alongside them and **no rendered frame of either has
ever been looked at**, by anyone.

They are not untested — they carry the strictest obligations in the contrast
suite, at a 7.00:1 text minimum against 4.50, and they pass. That is exactly
what makes this worth a card rather than a note.

## Why passing the sweep is not the same as being right

The dark scheme also passed every contrast obligation while rendering its
scrollbars as bright white troughs, because a scrollbar trough is not a
contrast pair and the sweep cannot see it. It went unnoticed for as long as the
probe could only produce light captures, and was found in the first dark frame
that existed.

The high-contrast profiles are now in precisely that position: measured,
compliant, and unobserved. Anything that is wrong with them is wrong in a way
the measurements are blind to, or it would have been caught already.

Two specific reasons to expect surprises rather than none:

1. Both profiles **invert the change-group pair** — the header background is
   solid black or white where light and dark use a pale wash. The boundary rule
   picks whichever of the two contrasts with the code surface, which is why it
   lands on the background there and on the text colour in the other two. That
   selection has never been seen rendered.
2. `high_contrast_light` has owed a non-colour signal for
   `syntax_warning` / `syntax_error` since it was written. Whether that reads as
   a problem on screen is unknown.

## Captured, 2026-08-08 — and they render correctly

`--diff-evidence-appearance` now accepts `high-contrast-light`,
`high-contrast-dark` and `high-contrast-system`, so all six modes can be
captured. First frames of both profiles are at
`docs/verification/reviews/high-contrast-scheme-acceptance-v001/`.

Nothing was changed to produce them. These are the profiles as they ship.

The two expectations this card recorded both resolved in the product's favour:

- **The inverted change-group pair renders correctly.** The boundary rule
  resolves to the header *background* in these profiles — solid black on light,
  solid white on dark — where light and dark resolve it to the header text
  colour. That branch had never been executed against a real surface. It picks
  the right one.
- **Selection strength is not the weakness it was in dark.** Distance from the
  pane measures 16.6 in high-contrast light and 12.5 in high-contrast dark,
  against 12.5 in the accepted light profile and 14.2 in the accepted dark one.
  Both are at or above what was approved.

So the honest outcome is that looking found no defect this time. That does not
retire the argument — the dark scrollbar was invisible to the sweep and visible
in the first frame, and the only way to know which case a profile is in is to
look. It does mean the two profiles enter attended review from a stronger
position than dark did.

## Accepted, 2026-08-08, both profiles, no notes

Batch `bpr:/attended-review/betterpullreview/3099091a38a163ff670dec72`, verdict
and package at
`docs/verification/reviews/high-contrast-scheme-acceptance-v001/`.

**All four shipped profiles are now reviewed surfaces.** That was not true of
any of them a week ago, and was not true of two of them this morning.

The card's argument stands even though it found nothing: the point was never
that these profiles were probably broken, but that nobody could say either way,
and that the contrast sweep is not the instrument that answers it. A white
scrollbar passed that sweep for as long as dark had no captures. These two
turned out fine, which is a result rather than a vindication of not looking.

## Superseded — required direct evidence

- A rendered frame of each profile, captured with
  `--diff-evidence-appearance`, carrying its own `appearance-at-capture.json`
  so the frame proves which profile it is.
- The same crops as the accepted light and dark batches, so the four can be
  compared on one axis.
- Presented for a verdict rather than filed. An unreviewed capture is the same
  gap one step later.

## Do not

Do not treat the contrast sweep passing as the evidence. That is the exact
inference that left a white scrollbar shipping in the dark profile, and this
card exists because it was wrong once already.
