---
schema: "implement-rfc.card.v1"
id: "BPR-2344"
title: "The Agent tab's report summary is a donut with a legend"
status: "In Progress"
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
