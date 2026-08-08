---
id: BPR-0209
title: The two high-contrast profiles have never been seen
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0201]
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

## Required direct evidence

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
