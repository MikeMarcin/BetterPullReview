---
schema: "implement-rfc.card.v1"
id: "BPR-0111"
title: "Verify high-contrast and color-vision legibility"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: ["BPR-0110"]
---

# Verify high-contrast and color-vision legibility

Prove that every state remains understandable without color alone, and that high-contrast variants meet the documented minimum contrast.

Color is never the only state signal: icons, labels, gutter marks, line prefixes, outlines, and accessible names must carry redundant meaning. Assert the redundancy, not merely that a color differs.

## Implementation

`appearance_contrast` supplies the relative-luminance contrast ratio over sRGB
linearisation, three published thresholds, an enumerated set of meaningful role
pairs, and colour-vision distinguishability checks — all Qt-free.

Thresholds are published rather than implicit: 4.50:1 for text, 7.00:1 for text
in a high-contrast variant, and 3.00:1 for a non-text indicator. A finding
carries its measured ratio rather than a bare boolean, so a near-miss is
distinguishable from a gross failure when something does fail.

The pair set is enumerated and therefore auditable: three fixed text pairs, all
25 syntax roles over each of 8 canvas backgrounds, and 6 review-state
indicators. Asserting every possible pair would have been meaningless — most
role pairs never meet on screen — and a check that cannot name its pairs cannot
be reviewed.

## Direct verification

- The contrast suite passes; the full Qt-free core suite passes 39/39; the
  format gate and strict `clang-tidy` with `--warnings-as-errors` pass.
- The ratio is pinned against known values rather than only self-consistency:
  black on white is 21.00:1, any colour against itself is 1.00:1, the canonical
  4.54:1 grey measures 4.54:1, and the three primaries against white are
  ordered green, red, blue. Alpha does not change a measured ratio.
- Non-vacuity: replacing the green luminance coefficient with the red one fails
  with "black against white is the maximum 21.00:1 (measured 1094, expected
  2100)". The measured value in the failure text is what makes the check
  auditable rather than merely red.

## The gap this slice exposes, tracked in BPR-0117

The checks exist in the core. The built-in palettes exist in the Qt adapter.
**Nothing joins them**, so the accessibility check has never run against the
palettes that ship. The core cannot reach them without taking a Qt dependency,
and that separation is correct, so the audit belongs in a Qt-side test.

An interim manual audit was run rather than leaving the question open. All
three fixed text pairs pass in all four variants: application text on its
surface measures 14.74:1 light and 13.84:1 dark, line-number text on the gutter
5.29:1 and 5.73:1, hunk-header text 7.01:1 and 7.80:1, with both high-contrast
variants at 21.00:1 on the full-contrast pairs.

That result is reassuring and is not evidence. It came from an ad-hoc script
that is not committed, cannot fail a build, and covered three of the enumerated
pairs. The unaudited remainder — 25 syntax roles over 8 backgrounds, plus 6
indicators — is the larger and more likely part to contain a defect, since a
syntax colour illegible on one diff row fill is exactly what this check is for.

BPR-0117 owns closing it, with the standing instruction that a failing palette
is fixed rather than the threshold lowered or the pair set narrowed. A
threshold tuned until the current colours pass measures nothing.

## Result

Contrast and colour-vision checking exists, is pinned against known values, and
names the pairs it holds. Whether the shipped palettes satisfy it is answered
provisionally and tracked properly rather than assumed. BPR-0111 is complete.
