---
id: BPR-0322
title: Restore colour-vision separation for high-contrast dark intraline emphasis
status: In Progress
rfc: RFC-BPR-0006
depends_on: [BPR-0320]
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
