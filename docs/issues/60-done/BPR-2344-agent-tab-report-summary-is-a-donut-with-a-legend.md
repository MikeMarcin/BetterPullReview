---
schema: "implement-rfc.card.v1"
id: "BPR-2344"
title: "The Agent tab's report summary is a donut with a legend"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2344--BPR-2344--8c919c90edb7.json","origin":"owner's request, 2026-11-21, replacing BPR-2336's segmented vertical bar","related_cards":["BPR-2336","BPR-2338","BPR-2331"],"related_rfcs":["RFC-BPR-0128"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6"]}
---

# BPR-2344: The Agent tab's report summary is a donut with a legend

## The owner's words

2026-11-21: "let's do a pie chart instead of a vertical line for report
findings in the agents tab."

[[BPR-2336]] made the summary under a report's attachment in the Agent tab
one segmented vertical bar, a label and count beside each segment; a press
opens the report at that label ([[BPR-2338]] fixed where the jump lands).

## Change

The bar becomes a small circular chart, with a legend beside it:

- A slice a label, in the report's order clockwise from twelve o'clock, in
  its label's tone (the theme's `report_verdict_*` roles; a neutral label in
  the window's ink at 45%), its angle exactly its share of the count.
- A legend beside it: one row a label with its colour swatch, its word and
  its count.
- A legend row and a slice open the report at the label's first item (a
  press; Space on a focused row). Hovering either highlights the slice and
  its row. Tooltip "Open the report at its first …" and accessible name
  "nitpick: 1" as before.

## Acceptance

- AC1. Each slice's angle is its count's share of 360 degrees, slices in
  report order without overlap or hole; a single-label report is one full
  circle.
- AC2. The legend has one row a label, in report order, each with its
  swatch in the slice's colour, its word and its count, whole and apart.
- AC3. The summary fits the narrowest drawer (280) and a wide one (640), at
  1.0, 1.25 and 2.0 scale and at a 1.6 times font.
- AC4. Pressing a slice, pressing a legend row, or Space on a focused row
  opens the report at that label's first item; the item then stands whole
  below the tab strip ([[BPR-2338]]).
- AC5. Hovering a slice or a row highlights both.
- AC6. `architecture_boundaries`, the format check and the documentation
  gate pass.

## Decisions

- The frontend-design skill was loaded. The chart is a donut, not a filled
  pie. Its hole holds the report's total, the one number the bar never
  showed. The ring keeps the 2 point gaps of the report page's meter as
  real gaps, cut along each edge, and the neutral label's translucent ink
  reads evenly in a ring. At 72 points it also stays light under the
  attachment card. It uses the theme's tones and no new palette.
- The donut keeps its size (72 points, 3 more each side for a lifted
  slice); the legend follows the font. A donut sized from the font would
  not fit the 280 point drawer at a 1.6 times font beside its legend.
- Legend rows share the legend's width; the counts stand at the right of
  each row, so they make one column. A word keeps its tone, as on the
  report page; the swatch carries the slice's colour.
- Hover lifts the slice 3 points out of the ring and washes its row. A row
  with keyboard focus lifts its slice too.
- A slice's tooltip is its row's, anchored to the slice through
  `anchor_tooltips` (the [[BPR-1354]] rule `architecture_boundaries`
  enforces). The donut takes no focus: the rows carry the keyboard and the
  accessible names, and the donut is named "Report summary: 6 in all".
- `report_tally_row` lost `segment_rect` and gained `swatch_rect` and
  `index`. `report_tally_pie` is new. `report_tally_view` lost the bar's
  constants and `band_height`/`bar_height` and gained `slice_start`,
  `slice_span`, `fill_of`, `highlighted`, `set_hovered`, `open` and `pie`.
  Nothing outside the view changed (one comment in `agent_tab_view.cpp`).
  `docs/architecture/ai-agent-boundary.md` describes the donut.

## Verification

macOS arm64, dev preset (Debug with `-gline-tables-only`), a task tree of
its own on main `e16ebad1a` plus this change. `format_check.py`
(clang-format 22.1.8) on every changed C++ file.

| Criterion | Check | Result |
|---|---|---|
| AC1 | `qt_agent_report_summary_scale_100/125/200` › `test_the_donut_is_exact` (one label: start 0, span 360, its tone at every 3 degrees round the ring; three to one: 270 and 90; each angle's half-degree on the ring belongs to its slice; the hole is no slice) and `expect_exact_slices` in the window (each start the previous end, each span `360 × count / total` within 1e-9, sum 360) | pass |
| AC2, AC3 | `qt_agent_report_summary_scale_*` › `expect_whole` at 280 and 640 points and a 1.6 times font: rows in report order, beside the donut, one under the other and as wide, swatch, word and count whole, apart and in order, counts in one column, each slice's middle in its tone; the summary needs 225 of 262 points at 280 and 257 of 262 at 280 with the larger font | pass |
| AC4 | `qt_agent_report_summary_scale_*`: a row's click, a press on the praise slice, Space on the focused nitpick row, each opening the report at that count's first item; in the standalone donut a press in the hole opens nothing; `test_a_jump_shows_its_item_whole_under_the_tab_header` with the suggestion opened through its slice, then every row and every menu section ([[BPR-2338]]'s placement) | pass |
| AC5 | `test_the_donut_is_exact`: a mouse move over the praise slice lights it (and it is drawn past the ring's edge), leaving lights nothing, entering the issue row lights the issue slice, a focused row lights its slice; a tooltip event on a slice shows its row's tooltip | pass |
| Not broken | `qt_agent_tone_colours`, `qt_agent_report_view`, `qt_drawer_surfaces_fit_scale_100/125/200`, `application_review_report_public_path`, `application_follow_up_public_path` | pass |
| AC6 | `architecture_boundaries` (it first refused the slice tooltip's `QToolTip::showText`; fixed with `anchor_tooltips`), `format_check.py`, `tidy.py` on `report_tally_view.cpp` (clean), `python3 scripts/verify_documentation.py --root .` | pass |

Looks: offscreen renders (`BPR_REPORT_SUMMARY_RENDER`) at 1.0 and 2.0 of
the narrowest drawer, a wide one, the narrowest at a 1.6 times font and a
single-label donut, kept outside the repository. No owner look was prepared
in this round; the owner's judgement of the donut is not recorded here. Not
run: Windows and Linux, a full `ctest`. `src/app` did not change, so no
composition digest was resealed.
