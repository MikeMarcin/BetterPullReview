---
id: BPR-0194
title: The selected line is invisible in Light, and a menu indicator escapes its button
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0183]
---

# The selected line is invisible in Light, and a menu indicator escapes its button

Two defects found by the first attended run of the native appearance probe on a
real display, 2026-08-06, with the product owner at the keyboard. Both were
measured from the captured rasters rather than judged by eye.

## 1. Selected-line contrast fails the non-text minimum in both schemes

Sampled from the left comparison pane, well clear of the glyphs, on the
selected row (`old-a`) against the row above it:

| Scheme | Selected | Unselected | Contrast |
|---|---|---|---|
| Light | `#f3f3f3` | `#ffffff` | **1.11:1** |
| Dark | `#404b59` | `#171717` | **2.02:1** |

WCAG's minimum for non-text user-interface components is **3:1**. Both fail.
Light fails so badly the band is not perceptible.

This is not a cosmetic complaint. The selected line is how a reviewer knows
which change they are reading, and the lower detail pane's "Change 1 of 3 →
old-a" is keyed to it. A reviewer navigating with *Next change* in Light mode
gets no visible confirmation of where they landed.

### Why the existing contrast work did not catch it

RFC-BPR-0006's ledger records `State survives without colour` as verified:
BPR-0117 ran the enumerated pairs against all four shipped palettes, found 191
failures, and repaired them by refitting lightness. That work was real and its
numbers are sound.

**Those four palettes are never applied.** BPR-0180 measured that the
application composes no `appearance_profile`, so what reaches the screen is
Qt's native palette, which BPR-0117 never examined. The contrast pass validated
palettes no user sees, while the palette every user sees carries a 1.11:1
selection indicator.

That is the composition gap producing a user-visible defect, rather than merely
an unshipped feature. It is the strongest argument yet for BPR-0183.

### What the fix must not be

Do not special-case the selection colour in the pane. The selection role
belongs to the appearance palette, and hard-coding a readable value in the
widget would leave the same hole open for every other role while making the
next reader believe the palette is in charge.

## 2. A menu indicator renders outside its button

The `Local history maintenance` menu button's dropdown chevron is drawn past
the button's own bottom-right border, outside the widget rectangle. The label
occupies the frame edge to edge with no elision and the indicator overflows.

Present identically in both schemes, so this is a layout defect and not a theme
one. It is visible in both captured rasters.

## Required direct evidence

- Selected-line contrast is at least 3:1 against the unselected background in
  every shipped scheme, measured from a captured raster by sampling pixels
  rather than by reading the palette that was supposed to be applied.
- The measurement runs against the palette the application actually composes.
  A test that reads a palette object proves nothing here — that is precisely
  how this was missed.
- The menu button's indicator lies inside its own frame at every supported
  width and scale, including the compact threshold.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Provenance

Captured by `--native-appearance-probe` with `--render-evidence` and
`--changed-render-evidence` on a real Cocoa/`QMacStyle` display, Light → Dark,
owner-authorised and owner-observed. The probe itself reported every marker OK;
these defects are outside what it asserts, which is why a person looking at the
output still mattered.
