---
id: BPR-0194
title: The selected line is invisible in Light, and a menu indicator escapes its button
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0183, BPR-0196]
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
| Light, after BPR-0183 composed the palette | derived | `#ffffff` | **1.23:1** |

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

### The cause, found by BPR-0183, is not the palette

The original diagnosis on this card was that the shipped palette was at fault
and that composing it would fix this. **That was wrong**, and BPR-0183 proved
it by composing the palette and re-measuring: selection contrast moved from
1.11:1 to 1.23:1. Still nowhere near 3:1.

The cause is `src/adapters/qt/src/diff_workbench.cpp`:

```cpp
auto color = aPane.palette().color(QPalette::Highlight);
color.setAlpha(72);
```

The band is the highlight colour at **28% opacity** over the pane background.
At that alpha the requirement is unreachable by any colour whatsoever:

| Pane | Best possible band | Contrast |
|---|---|---|
| light `#ffffff` | pure black at 28% → `#b7b7b7` | **2.01:1** |
| dark `#171717` | pure white at 28% → `#595959` | **2.56:1** |

So no palette change can satisfy 3:1 while `setAlpha(72)` stands. Refitting
`code_selection_background` — the obvious fix, and the one this card originally
prescribed — would have been work that could not possibly succeed, and the
measurement would have come back barely moved.

### What the fix must not be

Do not special-case the selection colour in the pane. The selection role
belongs to the appearance palette, and hard-coding a readable value in the
widget would leave the same hole open for every other role while making the
next reader believe the palette is in charge.

## Rejected at attended review on 2026-08-06, and why the question was wrong

All three subjects of the `selection_contrast_remedy` batch
(`bpr:/attended-review/betterpullreview/e50fdb53f4dc541069051598`) were
rejected. Evidence:
`docs/verification/reviews/selection-contrast-remedy-v001/`.

Two candidates cleared the mechanical gate — an opaque band at 3.69:1 and a 67%
wash at 3.04:1, both measured from captured pixels. The owner rejected them
anyway, on grounds the batch had not anticipated:

> this looks like a list box not a document view

BPR-0196 confirms that is a real defect and not the fixture text: the panes make
one `setBackground` call in the entire file and use zero of the palette's 45
semantic roles, so added lines, removed lines, intraline spans, moved code, and
hunk headers are all unpainted.

That invalidates the question this card asked. The choice between a band and a
wash is entirely about **what the marking obscures** — and it currently obscures
nothing, because nothing is painted underneath it. Neither candidate could be
judged on its merits, and offering them was a mistake in how the decision was
framed, not in how it was measured.

The reference behaves like the wash, not the band. The owner observed that
p4merge tints a multi-line selection while the syntax colouring survives
underneath — which is the candidate they called "best in terms of the
selection" before rejecting the batch on other grounds. Weigh that when this is
re-presented; the opaque band is no longer the option with a reference behind
it.

**This card is blocked behind BPR-0196.** Re-present the selection treatment
once a selected line has an added-line tint, intraline emphasis, and syntax
colour beneath it to compete with.

### DEFERRED CLASS: owner-attended, for defect 1 only

Not because the measurement is unclear — it is exact — but because the remedy
is a visible product choice with a real cost either way, of the same kind as
BPR-0118's line-height decision.

The two honest routes, both of which reach 3:1:

1. **Opaque or near-opaque band.** Simplest, and it dims or hides the syntax
   colouring of the very line the reviewer is reading. On a code surface that
   is a real loss, and it is the line they are most likely to be studying.
2. **Keep the translucent fill and add a full-opacity non-colour marker** — a
   left edge bar or outline. RFC-BPR-0006 already requires state to survive
   without colour, so this direction is mandated rather than invented, and the
   marker alone can carry the 3:1. It costs custom painting in a surface whose
   bounded-widget behaviour is load-bearing for RFC-BPR-0005's virtualization
   evidence.

An implementer should not pick between these silently. Defect 2, the menu
indicator escaping its button, is a plain layout bug with no such fork and
needs no decision.

Do not simply raise the alpha until the number passes. A full-opacity band
hides the syntax colouring of the line it marks, which is a different defect on
the same surface. The honest options are a lower alpha plus a non-colour
indicator (a gutter mark or an outline, which RFC-BPR-0006 already requires for
state anyway), or an opaque band with a foreground the palette guarantees
against it. Choose deliberately and record which.

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
