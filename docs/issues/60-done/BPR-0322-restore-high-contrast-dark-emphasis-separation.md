---
schema: "implement-rfc.card.v1"
id: "BPR-0322"
title: "Restore colour-vision separation for high-contrast dark intraline emphasis"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0320"]
---

# Restore colour-vision separation for high-contrast dark intraline emphasis

[[BPR-0320]]'s attended batch asked one question — how strongly should changed
characters be marked — and disclosed exactly one cost, the text contrast each
candidate spends. It did not measure colour-vision separation, and in the
high-contrast dark profile that omission cost something.

The accepted pair is `#04580F` added and `#750115` removed. Under deuteranopia
they sit 5.7 apart against the published floor of 10.0, so a deuteranope cannot
tell an added intraline span from a removed one in that profile. The pair the
palette carried before cleared the floor.

This is recorded as an obligation in
`tests/adapters_qt/appearance_profile_tests.cpp` rather than repainted away,
because repainting it would put a colour on screen that no verdict covers. The
core's own doctrine is that a pair colour cannot separate is a requirement on
the presentation layer, not a palette failure — but this project does not
currently paint any non-colour signal for intraline emphasis, so the obligation
is a debt until this card discharges it.

It matters more here than it would elsewhere. A high-contrast profile is
selected by someone who already found the default insufficient, so a
colour-vision regression lands on exactly the people least able to absorb it.

## What was already established

A substitute pair exists and was found while investigating: `#005200` added and
`#782A3E` removed holds the accepted strength (separations 20.0 and 22.3),
improves text contrast to 5.07:1 and 5.09:1 from 4.65:1 and 6.32:1, and
restores dichromat separation to 21.6 against the 10.0 floor.

It was deliberately not applied. It moves the removed emphasis from red to
plum, which is a visible change to a colour the product owner selected from
rendered captures, and substituting it silently would defeat the point of
having asked.

## Required final state

- The high-contrast dark intraline pair clears
  `distinguishable_difference_minimum_tenths` under typical vision,
  protanopia, and deuteranopia.
- Its separation from the row fills stays at or above the level BPR-0320
  accepted, and its text contrast does not fall below the accepted 4.65:1.
- Whichever route is taken — the substitute pair, a different pair, or a
  non-colour signal — the outcome carries an attended verdict, because it
  changes a colour a previous verdict fixed.
- The recorded obligation is removed from the reviewed list only when the
  measurement says it is discharged, never to make the audit quiet.

## Options, for whoever picks this up

1. Adopt the substitute pair and take a new attended verdict on the changed
   high-contrast dark appearance.
2. Keep the accepted colours and add a non-colour signal to intraline emphasis,
   which discharges this obligation and every future one of its kind. This
   overlaps [[BPR-0321]]'s derivation work and may be cheaper done together.
3. Re-run the strength selection for this one profile with colour-vision
   separation presented alongside contrast, which is the measurement the
   original batch should have carried.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure -R '^(core_appearance_contrast|qt_appearance_profile)$'
python3 scripts/verify_documentation.py --root .
```

The audit must report zero unreviewed obligations with the high-contrast dark
intraline entry removed from the reviewed list.

## Attended result — all three rejected, 2026-08-17

Every option in batch
`bpr:/attended-review/betterpullreview/f1db1ba708cfed928b632d9f` was rejected:

> None of these looks visible to me. It's supposed to be high contrast but
> these are the least contrasted examples. Might need to do something different
> than just darkening the wash for high contrast.

That reframes this card. It was written as a colour-vision repair, and the
answer is that the high-contrast dark intraline marks are not visible to
ordinary vision either — the profile a person selects *because* they need more
contrast is showing the least of any. Choosing between three tints that all
fail the plainer test would have been the wrong question, and the rejection is
the right answer to it.

The colour-vision defect this card was opened for is still real and still
recorded in the reviewed-obligation list. It is now a symptom of the same cause
rather than an independent problem: a tint at the published separation is not a
high-contrast treatment. [[BPR-0323]] owns the treatment; this card stays open
for the obligation and closes when the measurement says it is discharged.

## Attended result — HA accepted, 2026-08-17

The product owner selected the bright hue-true pairing from batch
`bpr:/attended-review/betterpullreview/8f175d37c3b12a3e17ca6fda`:

| | Added | Removed |
|---|---|---|
| High-contrast Light | `#55FF55`, black text at 15.82:1 | `#FF9999`, black text at 10.26:1 |
| High-contrast Dark | `#55FF55`, black text at 15.82:1 | `#FF9999`, black text at 10.26:1 |

The two unchosen pairings — cyan/amber and deep-with-white-text — are recorded
as not chosen rather than rejected.

Both fills carry their own text colour, which is what lets them be this bold: a
mark strong enough to see in a high-contrast profile cannot also let that
profile's syntax colours read on it. `emphasis_needs_own_foreground` answers
that question from the palette, and the contrast audit measures the foreground
actually painted rather than a combination the panes never produce.

This card's original defect is discharged. The pair stays 36.4 apart under the
worst colour-vision model against a floor of 10.0, where the previous pair sat
at 5.7. The reviewed-obligation list loses its high-contrast dark intraline
entry because the measurement says so, not to quiet the audit.

The acceptance was qualified — "This is great other than the selected row" — so
the selected-row treatment remains open in these profiles exactly as it does in
the standard ones. [[BPR-0323]] owns it, and this card stays open until that
lands, because a profile is not finished while one of its row states is wrong.

## Closed — 2026-08-17

Both halves are now settled. The accepted pairing `#55FF55` and `#FF9999`
discharges the deuteranopia defect this card was opened for, at 36.4 against a
floor of 10.0. The selected-row qualification on that acceptance — "great other
than the selected row" — is answered by [[BPR-0323]]'s hue-preserving mark,
accepted in batch
`bpr:/attended-review/betterpullreview/d9a2e27adfc4279abdce0eaf`.

The card was opened as a colour-vision repair and that framing was too narrow.
The product owner's observation that the high-contrast profiles were "the least
contrasted examples" showed the marks were failing ordinary vision too, in the
profile people select precisely because they need more. The colour-vision
defect was a symptom. Choosing between three tints that all failed the plainer
test would have been the wrong question, and the rejection that said so is the
most useful result this card produced.
