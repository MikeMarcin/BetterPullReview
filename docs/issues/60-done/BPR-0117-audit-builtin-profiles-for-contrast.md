---
schema: "implement-rfc.card.v1"
id: "BPR-0117"
title: "Audit the built-in profiles against the contrast checks"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0111"]
---

# Audit the built-in profiles against the contrast checks

BPR-0111 supplies the contrast and colour-vision checks in the Qt-free core.
BPR-0106 supplies the four built-in palettes in the Qt adapter. **Nothing joins
them**, so the accessibility check has never run against the palettes that
actually ship.

The core cannot reach the palettes, because they live in the Qt adapter and the
core takes no Qt dependency — that separation is correct and should not be
undone. The audit therefore belongs in a Qt-side test that links both.

An interim manual audit computed the three fixed text pairs for all four
profiles. Every one passes: application text on its surface measures 14.74:1
light and 13.84:1 dark; line-number text on the gutter measures 5.29:1 and
5.73:1; hunk-header text on its background measures 7.01:1 and 7.80:1. Both
high-contrast variants measure 21.00:1 on the two full-contrast pairs and about
12:1 on the gutter. That is reassuring but it is not evidence: it was computed
by an ad-hoc script that is not committed, cannot fail a build, and covers only
three of the enumerated pairs.

The unaudited remainder is the larger part: 25 syntax roles over 8 canvas
backgrounds, and 6 review-state indicator roles. A syntax colour that is
illegible on one of the diff row fills is exactly the defect this check exists
to find, and it is precisely the case the manual audit skipped.

Wire the enumerated pair set to `builtin_appearance_palette` for every variant,
and assert each finding against its published threshold. If a built-in palette
fails, **fix the palette** — do not lower the threshold and do not narrow the
pair set to exclude the failure. A threshold tuned until the current colours
pass measures nothing.

## Required direct evidence

- Every enumerated pair is evaluated against all four built-in variants, with
  the measured ratio reported for a failure rather than a bare boolean.
- The colour-vision distinguishability check runs against the added and removed
  diff roles of each variant.
- Any palette change made to satisfy a threshold is recorded with its before
  and after ratio.
- The assertion is shown to fail when a palette is perturbed below its
  threshold.

## Outcome

The audit was wired to `builtin_appearance_palette` in
`tests/adapters_qt/appearance_profile_tests.cpp` and **the shipped palettes
failed it**: 191 of the enumerated pairs missed their published minimum — 41 in
Light, 28 in Dark, 41 in High Contrast Light, and 81 in High Contrast Dark. The
manual audit had missed all of them, because it checked only the three fixed
text pairs and every failure was a syntax colour on a *fill*.

Every failure had one shape. Contrast depends only on relative luminance, so
per variant the whole audit collapses to a band: on a light canvas each fill
must sit at or above a floor and each syntax colour at or below
`(floor + 0.05) / minimum - 0.05`; on a dark canvas the inequalities invert.
Syntax colours tuned to pass on the canvas alone leave *no* headroom, because a
white canvas is already the maximum luminance — so any fill drawn over it
necessarily fails. The palettes had never been designed against that
constraint.

The fix moves 30 colours, holding each one's hue and saturation and refitting
only its HSL lightness, so the profiles keep their identity:

| Variant | Fill band | Colours moved | Failing pairs before / after |
|---|---|---|---|
| Light | fills L >= 0.80, syntax L <= 0.1389 | 3 fills, 8 syntax | 41 / 0 |
| Dark | fills L <= 0.030, syntax L >= 0.3100 | 3 fills, 1 syntax | 28 / 0 |
| High Contrast Light | fills L >= 0.80, syntax L <= 0.0714 | 4 fills, 3 syntax | 41 / 0 |
| High Contrast Dark | fills L <= 0.030, syntax L >= 0.5100 | 4 fills, 4 syntax | 81 / 0 |

No threshold was lowered and no pair was removed from the checked set.

### The emphasis fills needed chroma, not lightness

Pinning fill luminance nearly introduced a second accessibility defect. On a
white canvas at the required luminance a fully saturated red *is* about
`#FFEBE9`, which is already `diff_removed_background` — so the first repair
made `diff_intraline_removed_background` identical to the row fill it marks,
silently erasing intraline emphasis while passing the contrast audit.

Luminance is spent on legibility, so the only freedom left for separating an
emphasis fill from its row is chroma. The intraline fills are now fully
saturated at the band limit and the row fills are held further toward the
canvas. `test_intraline_emphasis_never_collapses_onto_its_row_fill` pins the
result; it is the only assertion that catches this, since the collapsed colour
passes every contrast check.

### Colour alone cannot carry red against green

The colour-vision check reports that `diff_added_background` against
`diff_removed_background` is **not** separable by colour alone in any variant —
in Light the two differ by 2.0 CIE76 units under deuteranopia against a
published minimum of 10.0. This is not a regression and not repairable by
repainting: both modelled dichromacies lose the red-green axis, and the RFC
specifies red/green diff fills.

The core states the correct reading directly: a false verdict "is not a defect;
it means the presentation layer must carry that distinction with an icon, a
line prefix, a gutter mark, an outline, or an accessible name". The ten such
pairs are recorded in `reviewed_color_only_obligations` and asserted exactly,
so a later palette edit that creates a *new* colour-only-indistinguishable pair
fails rather than passing quietly.

Saturating the emphasis fills also **removed** three of these obligations:
`diff_intraline_added_background` against `diff_intraline_removed_background`
was indistinguishable in Light, Dark, and High Contrast Dark before this change
and is separable in all three now. The baseline was measured by reverting only
the palette file and re-running, so the 13-to-10 improvement is a measurement
rather than a claim.

### Non-vacuity

Each assertion was broken separately, rebuilt, and observed to fail for its own
reason, then restored to green:

- High Contrast Dark `code_selection_background` returned to `#004CB4` — 23
  contrast failures, "every built-in profile meets the published minimum".
- One recorded obligation deleted — "no reviewed non-colour obligation has
  silently disappeared".
- Light `diff_intraline_removed_background` set to the row fill — "an intraline
  emphasis fill differs from its row fill", with the contrast audit still
  passing, which is the point of that assertion.
