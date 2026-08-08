---
id: BPR-0206
title: Refit the light syntax palette so a visible selection wash is legible
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0196]
---

# Refit the light syntax palette so a visible selection wash is legible

Route 1 of [[BPR-0204]], chosen by the product owner on 2026-08-07:

> Route 1, we want to do the normal selection blue tint that everyone else
> manages to accomplish.

Candidate C — `code_selection_background` `#7FB0E8` — is now applied, and the
light syntax colours are refitted so it is legible.

## The reference settles the shape

Git Fork's light scheme was supplied as the target. A first reading of it was
wrong and the owner corrected it:

> I don't think it's fair to call it replaced, I can clearly see here a
> selected green and selected red and selected white

Correct. Fork's selection is a **translucent blue wash** and the change tints
survive underneath it — the same shape this product already has, and the same
shape in Fork's dark scheme. So the wash stays and `selection_wash_alpha`
stays at 72. What had to move was the syntax palette.

## What was wrong

The light syntax colours were fitted against near-white backgrounds. A wash
dark enough to see drops the composite's luminance to roughly 0.68, which costs
about a third of every contrast ratio. At `#7FB0E8` that put **53 pairs** below
the 4.50 minimum, worst 3.82:1.

It was not a wash-versus-opaque question either. An opaque Fork-like
`#B9DFE7` measured 3.91:1 against the same palette. Nothing about the marking's
mechanism could fix a foreground palette that had no headroom.

## The refit

Eleven of the twenty-five light syntax roles sat above the luminance budget.
Each was scaled down uniformly **in linear RGB**, which multiplies luminance by
a constant and leaves chromaticity untouched — the hue and saturation of every
token category are exactly what they were, only darker.

| role | from | to |
|---|---|---|
| `syntax_attribute` | `#8D5F00` | `#7E5500` |
| `syntax_comment` | `#5B6570` | `#555E69` |
| `syntax_documentation` | `#4A6B57` | `#456351` |
| `syntax_escape_sequence` | `#A45100` | `#944800` |
| `syntax_import` | `#197834` | `#156B2E` |
| `syntax_information` | `#0964D1` | `#0759BC` |
| `syntax_keyword` | `#CA212D` | `#B51D27` |
| `syntax_region_marker` | `#57606A` | `#555E68` |
| `syntax_string` | `#0A7752` | `#086A49` |
| `syntax_type` | `#7B46DD` | `#6E3EC8` |
| `syntax_warning` | `#B14700` | `#9F3F00` |

The budget came from the tightest background: the selection composited over
`diff_intraline_removed_background`, luminance 0.676. Each colour was bisected
down to 4.55:1 there — a small margin over 4.50 deliberately, because
[[BPR-0204]] recorded how little headroom the old arrangement had.

Only the light profile changed. Dark, high-contrast light and high-contrast
dark were already clear: their worst washed pairs measure 6.06, 8.19 and 10.41.

## Selection visibility after the refit

ΔE76 of the selected line against the band beneath it:

| | pane | removed | added |
|---|---|---|---|
| Light | 12.5 | 13.2 | 13.3 |
| Dark | 7.3 | 13.0 | 10.1 |

Light now sits comfortably past the ~10 that counts as distinct, against the
5.2 that produced "I can only see it with eye strain".

**Dark over the plain pane is 7.3 and has not been reviewed.** It is the one
place a selection is still weaker than the light profile, and no owner verdict
covers it. It is also not capturable yet — see [[BPR-0201]].

## One obligation was accepted rather than engineered away

Darkening `syntax_warning` brought it close enough to `syntax_error` that
deuteranopia can no longer separate them, so the light profile now owes a
non-colour signal for that pair. Recorded in
`reviewed_color_only_obligations`.

This was not the first choice. Amber-shifted warnings at `#765900` and
`#6D5C00` were both tried and both still failed: at the luminance a selected
line demands, deuteranopia collapses dark amber and dark red regardless of hue.
The high-contrast light profile has owed this exact pair since it was written,
for the same reason.

The register exists for precisely this, and RFC-BPR-0006 already requires a
non-colour indicator, so the obligation is dischargeable. It is recorded rather
than dodged by keeping the tokens too light to read when selected — which is
the trade this whole card exists to stop making.

## Verification

`qt_appearance_profile`, `core_appearance_contrast` and `core_appearance_policy`
pass with `#7FB0E8` applied. The contrast sweep now measures the wash as
composited, per [[BPR-0204]], so these are ratios against pixels the panes
actually produce.

Rendered evidence at the same fixture and crop as the review batch shows the
selected line clearly separated from the removed block below it.

## Still open

- The dark profile's selection over a plain pane, at ΔE 7.3, has no verdict.
- One acceptance criterion from the batch — whether the change tint reads
  through the wash — remains unanswerable from single-line captures.
  [[BPR-0205]].
