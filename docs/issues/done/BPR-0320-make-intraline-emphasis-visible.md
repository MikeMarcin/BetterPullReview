---
id: BPR-0320
title: Make intraline emphasis visible on both sides in every profile
status: Done
rfc: RFC-BPR-0006
depends_on: []
---

# Make intraline emphasis visible on both sides in every profile

The product owner rejected attended batch `uiforetw-intraline-acceptance-v002`
partly for "missing corresponding source-pane character emphasis while the
destination span is highlighted". That reads like a painting bug. It is not.
The painter is symmetric: `diff_workbench::append_intraline_emphasis` runs once
per pane and colours the removed side from `Colors.IntralineRemoved` exactly as
it colours the added side from `Colors.IntralineAdded`. The core is symmetric
too — `intraline_ranges` computes both sides from one pass and pushes them as
one record.

The defect is in the built-in palettes. Measured as CIE76 ΔE against each
fill's own row background, at the current head:

| Profile | Added | Removed |
|---|---:|---:|
| Light | 38.30 | **4.23** |
| Dark | **8.77** | **7.60** |
| High-contrast Light | 38.03 | **4.16** |
| High-contrast Dark | **4.27** | 19.24 |

Reading the fresh captures confirms every number. In Light the added pane marks
`24 36 48 60 72` in obvious green and the removed pane shows no mark at all. In
Dark neither side is visible. The owner saw the Light asymmetry and reported it
accurately; the Dark collapse is the same defect and nobody has reported it
because, as [[BPR-0214]] establishes, intraline emphasis has never once
appeared in a reviewed capture.

Why the removed side lost: the required contrast audit pins every fill's
luminance, and green carries 0.7152 of luminance against red's 0.2126. The
added side can move a long chromatic distance at nearly constant luminance by
holding G at maximum and dropping R and B. The same move on the removed side
costs far more luminance, so each tightening of the audit pushed the removed
fill back toward its row colour until the two collapsed.

## Why the existing checks did not catch it

- `test_intraline_emphasis_never_collapses_onto_its_row_fill` compares the two
  colours with `!=`. `FFEBE9 != FFE2DF` passes trivially. The test's own
  comment states the real intent — "collapsing the intraline emphasis onto the
  row it marks would trade one accessibility failure for another" — and the
  assertion does not encode it.
- `test_intraline_ranges_match_the_model` asserts emphasis only on
  `workbench.right_pane()`, only `diff_intraline_added_background`, only
  `edit.Right`. The removed side is asserted nowhere.
- `core_appearance_contrast` holds text pairs to a contrast ratio and
  hue-critical pairs to a ΔE floor, but nothing requires a fill to be
  distinguishable from the fill it is painted over.

## Required final state

- The core publishes an emphasis-separation policy: the enumerated pairs whose
  members are painted one over the other, a published minimum, and a
  measurement function. The threshold is a core decision, not a Qt opinion.
- Every built-in profile meets that minimum on both the added and the removed
  side.
- Neither side of a profile is disproportionately weaker than the other. A
  reviewer must not be able to tell which pane the tool considers important.
- The existing required-contrast audit and every hue-critical obligation still
  pass unchanged. Emphasis visibility may not be bought with text legibility.
  *Amended by the attended decision; see "Two amendments" below. Both halves of
  this bullet were overtaken by the accepted candidate, and the exact scope and
  provenance of each departure is recorded there rather than here.*
- A focused test fails before the palette change and passes after, for the
  exact pairs measured above.
- The Qt model test asserts the removed side as well as the added side.

## Non-goals

- Do not change intraline range computation, mapping, or the semantic roles.
- Do not weaken `text_contrast_minimum_hundredths`,
  `high_contrast_text_minimum_hundredths`, or any hue-critical floor.
- Do not change connector, selection, or navigation-icon rendering; those are
  [[BPR-0259]] and [[BPR-0258]] and are separately accepted or pending.
- Do not claim an attended verdict. This card makes the emphasis visible; the
  batch that judges it belongs to [[BPR-0233]].

## Correction — the ceiling is a policy, not physics

An earlier reading of this card claimed fill colour could not carry the removed
side at all. That was wrong, and the product owner's reference diff disproved
it. Measured from the reference image: its removed row is `#FAE1E1` and its
removed emphasis is `#F4B3B2`, a separation of 20.6 — five times ours.

It reaches that by not holding the emphasised span to 4.50:1. Over `#F4B3B2`
the lightest syntax roles measure 3.74:1 bare and 3.41:1 under the selection
wash. So the real constraint is this repository's own decision to apply the
WCAG AA text floor to intraline emphasis fills, not a property of the colour
space.

What the colour space does force is the asymmetry: green carries 0.7152 of
relative luminance and red 0.2126, so the added side buys chromatic distance
almost free while the removed side pays for it in luminance. Holding 4.50:1
with a recognisable pink row caps the removed side near 8.4. Letting the row go
nearly white (`#FEFEF8`) reaches 15.7 and destroys the row's identity.

The trade is therefore real and unavoidable, and where to sit on it is a
product decision rather than an engineering one.

## Attended checkpoint — strength selection pending

Five alternatives were rendered through the real UIforETW selected-diff course
at 1280 × 1000 in all four appearance profiles, one per candidate, every
capture emitting `UIFORETW_SELECTED_DIFF_EVIDENCE_OK`. They form one
choose-exactly-one selection group, so the four not chosen are recorded as not
chosen rather than as defective.

| Candidate | Light removed | Separation | Tightest syntax role |
|---|---|---:|---|
| C0 keep current | `#FFE2DF` | 4.2 | 4.55:1 |
| C1 subtle | `#F4D5D2` | 8.1 | 4.16:1 |
| C2 moderate | `#E2C3C0` | 14.0 | 3.64:1 |
| C3 reference-matched | `#F1B9B4` | 20.7 | 3.50:1 |
| C4 strong | `#F9AEA8` | 28.2 | 3.36:1 |

Every candidate leaves the row fills untouched and never weakens a side that
already works, so the light and high-contrast-light added fills stay at 38.3
and 38.0 in all five.

The batch is
`bpr:/attended-review/betterpullreview/ebe2ee306b89f453d1de6704` at
[`intraline-emphasis-strength-v001`](../../verification/reviews/intraline-emphasis-strength-v001/offline-package/index.html).
The selected candidate sets both published constants, which is why
`emphasis_separation_minimum_tenths` and
`emphasis_balance_maximum_hundredths` are not committed yet: the current
working values of 7.0 and 2.50 were chosen before the captures existed, and the
captures show 7.5 and 8.7 are invisible in the dark profile. No verdict is
inferred and no palette is committed until the exact export is validated.

## Attended result — C3 accepted, 2026-08-16

The product owner selected `intraline-emphasis-c3` from the five alternatives.
The export validated against its batch manifest and is recorded at
`docs/verification/reviews/intraline-emphasis-strength-v001/`, with the four
unchosen candidates recorded as not chosen rather than rejected.

Shipped palette:

| Profile | Added | Removed | Separations | Balance |
|---|---|---|---:|---:|
| Light | `#9EFFB5` unchanged | `#F1B9B4` | 38.3 / 20.7 | 1.85 |
| Dark | `#065730` | `#520412` | 20.6 / 21.2 | 1.02 |
| High-contrast Light | `#99FF99` unchanged | `#F3B9BA` | 38.0 / 20.3 | 1.87 |
| High-contrast Dark | `#04580F` | `#750115` | 20.1 / 20.5 | 1.01 |

Both published constants are pinned to that palette rather than guessed:
`emphasis_separation_minimum_tenths` is 200, its weakest pair, and
`emphasis_balance_maximum_hundredths` is 200, above its widest 1.87. The first
draft of this card set them to 70 and 250 from reasoning about
just-noticeable differences, and the captures showed the dark profile passing
70 at 7.5 and 8.7 while displaying nothing. The numbers now come from
observation.

### Two amendments to the required final state

The bullet requiring the existing audit to pass unchanged did not survive
contact with the decision, and is recorded as amended rather than quietly
dropped.

**The text floor was lowered, within an exact scope.** Text on the two
intraline emphasis fills is now held to `emphasis_text_contrast_minimum_hundredths`
(3.50:1) and `high_contrast_emphasis_text_minimum_hundredths` (4.65:1) through
a new `contrast_kind::emphasis_text`. Every other background, including the
diff row fills those two sit inside, keeps the full 4.50:1 and 7.00:1 floors,
so ordinary code is untouched and only the changed characters inside an
already-highlighted row read at the lower ratio. The attended page disclosed
all five breach values for this candidate before it was chosen: Light removed
3.50:1, Dark added 3.93:1, High-contrast Light removed 5.15:1, High-contrast
Dark added 4.65:1, High-contrast Dark removed 6.32:1.

**One colour-vision obligation moved, net neutral in count and not in kind.**
The dark intraline pair was already recorded as needing a non-colour signal;
this palette discharges it. The high-contrast dark pair now needs one, sitting
5.7 apart under deuteranopia against a floor of 10.0. That is a regression the
batch never measured, and [[BPR-0322]] owns it with a known substitute pair
already found and deliberately not applied, because it would change a colour
the owner selected.

### Carried out of the verdict

The accepting note reported a defect the batch did not present: intraline
emphasis keeps its added or removed hue when the row wears the selection wash
or the moved band. [[BPR-0321]] owns it. The captures show it plainly — the
selected top row carries a blue wash and a salmon patch at the same time.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_appearance_contrast|qt_appearance_profile|qt_diff_workbench_scale_(100|125|200)|application_uiforetw_selected_diff_contract)$'
ctest --preset core-only --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```
