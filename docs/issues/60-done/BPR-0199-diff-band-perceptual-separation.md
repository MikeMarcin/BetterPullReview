---
id: BPR-0199
title: Diff bands and change-group markings are too close to the pane to read apart
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0196, BPR-0198]
---

# Diff bands and change-group markings are too close to the pane to read apart

Found while investigating why the second attended review of BPR-0198 was
unanswerable. The captures were cropped wrongly — that is
[[BPR-0198]]'s own defect and is recorded there — but measuring the palette to
explain the confusion turned up a separate, real problem underneath.

## Luminance ratio is the wrong instrument here, and it produced a false alarm

Measured against the pane background, WCAG-style:

| Band | Light vs `#FFFFFF` | Dark vs `#0D1117` |
|---|---|---|
| added | 1.06:1 | 1.30:1 |
| removed | 1.15:1 | 1.16:1 |
| moved | 1.12:1 | 1.42:1 |
| change-group header | 1.17:1 | 1.17:1 |

Every one is far below the 3:1 that WCAG 1.4.11 sets for non-text UI. On that
reading the entire diff palette is broken.

**That reading is wrong, and it is worth recording why so the next person does
not repeat it.** WCAG contrast is a ratio of *relative luminance* only. It is
blind to hue and chroma. Two backgrounds can be identical in luminance and
still be told apart instantly if they differ in colour — which is exactly what
a diff tint does. Applying the 3:1 non-text threshold to a full-width
background wash measures the wrong property and condemns palettes that work.

The right instrument is perceptual distance. CIELAB ΔE76 against the pane, with
ΔE ≈ 2.3 the just-noticeable difference and ≈ 10 comfortably distinct:

| Band | Light ΔE | Dark ΔE |
|---|---|---|
| added | 13.4 | 24.5 |
| removed | **9.1** | 23.8 |
| moved | 15.4 | 30.8 |
| change-group header | **8.7** | **9.9** |

And added-vs-removed side by side, which is the discrimination that actually
matters when reading a replacement: Light **18.5**, Dark **36.6**.

## What is actually wrong

Three values, not the whole palette:

1. **Light `diff_removed_background` `#FFEBE9`, ΔE 9.1, chroma 7.3.** The
   weakest band that carries meaning. It is the one a reviewer must
   distinguish from an unchanged line while scanning, and it sits at the
   noticeable-difference boundary.
2. **Light `diff_hunk_header_background` `#E7EEF9`, ΔE 8.7, chroma 6.1.** The
   marking that separates one change group from the next.
3. **Dark `diff_hunk_header_background` `#18212F`, ΔE 9.9, chroma 10.6.** Same
   marking, same problem, in the other scheme — and conspicuously the only
   Dark value that is weak while its neighbours sit at 24–31.

The Dark added/removed/moved bands are **fine** and should not be touched.

## Dark-mode reference: Git Fork

Supplied by the product owner, 2026-08-07, as a target for dark-mode diff
colouring. Sampled approximately by eye from the reference screenshot — these
are **estimates, not measured values**, and should be re-sampled from a real
capture before anyone fits to them:

| | approximate | ΔE vs pane |
|---|---|---|
| pane | `#2B2B2B` | — |
| added | `#3A5236` | 25.3 |
| removed | `#5C3A3A` | 19.6 |
| added vs removed | | 31.6 |

The useful finding is that **our Dark bands already match or beat the
reference** — 24.5/23.8 against Fork's 25.3/19.6, and 36.6 side-by-side
separation against Fork's 31.6. Fork's bands look decisively better than ours
in Light not because dark mode is intrinsically better but because Fork commits
chroma to the band. Our Light profile does not.

Note also what the reference does *not* do: Fork draws no tinted
change-group header at all. It separates groups with structure — a rule and a
gutter break — rather than a wash. That is worth weighing against strengthening
value 2 and 3 above, and is a genuine fork in the design rather than a defect
to repair.

## Accepted on 2026-08-07, both schemes, no notes

Batch `bpr:/attended-review/betterpullreview/236327ff3e80f7d487e7c187`.
Evidence: `docs/verification/reviews/diff-scheme-acceptance-v002/`. Both
subjects accepted with no further comment, after three rounds in which every
subject had been rejected.

What this card asked for is answered. The bands separate, the change-group
marking is a rule rather than a fill, and the dark scheme is rebuilt at the
reference's own lightness and chroma offsets — measured from the owner's
screenshot in LCh and applied to our pane rather than copied as hex, since ours
is twelve L* darker.

**What it does not cover.** The two high-contrast profiles have still never had
a rendered frame reviewed, and the batch said so. They are not covered by this
acceptance and the palette work behind them is untested by any human eye — the
same gap that hid a white scrollbar in the dark scheme for as long as dark had
no captures. That is [[BPR-0209]].

## Superseded — required direct evidence

- Each band's ΔE76 against its own pane background, in every shipped scheme,
  computed from **pixels sampled out of a captured raster**, not from the
  palette object. Reading the palette is how the composition gap in
  [[BPR-0194]] went unnoticed for so long.
- Added and removed measured against **each other**, not only against the pane.
  A reviewer's real discrimination task is telling the two sides of a
  replacement apart.
- The change-group marking is shown to be locatable without relying on its
  fill — whether that is by strengthening the fill or by replacing it with a
  rule, record which was chosen and why.
- Each new assertion is shown to fail when the value it constrains is moved
  back.

## Do not

Do not fit these to a WCAG 3:1 luminance target. That is the wrong metric for a
background wash, and hitting it would force bands so dark they would swamp the
syntax colouring underneath — the same defect [[BPR-0194]] is trying to avoid
for the selection band, arriving from a different direction.
