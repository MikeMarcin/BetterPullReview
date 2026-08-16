---
id: BPR-0320
title: Make intraline emphasis visible on both sides in every profile
status: In Progress
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

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(core_appearance_contrast|qt_appearance_profile|qt_diff_workbench_scale_(100|125|200)|application_uiforetw_selected_diff_contract)$'
ctest --preset core-only --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```
