---
id: BPR-0117
title: Audit the built-in profiles against the contrast checks
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0111]
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
