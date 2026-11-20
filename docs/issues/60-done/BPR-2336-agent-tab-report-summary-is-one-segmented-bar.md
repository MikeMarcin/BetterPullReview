---
schema: "implement-rfc.card.v1"
id: "BPR-2336"
title: "The Agent tab's report summary is one vertical bar of coloured segments"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2336--BPR-2336--6719737576bd.json","origin":"owner's attended review b64bbc238073ff3b665048b611ef1c60 (frozen from main d802ef303), Reject on BPR-2331's Agent tab report summary","related_cards":["BPR-2331","BPR-2166","BPR-2296"],"related_rfcs":["RFC-BPR-0128"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2336: The Agent tab's report summary is one vertical bar of coloured segments

## The owner's words

Attended review session `b64bbc238073ff3b665048b611ef1c60`, frozen from main
`d802ef303`, rejected [[BPR-2331]]'s summary under a report's attachment in
the Agent tab:

"I wish this was a single vertical bar with colored segments instead."

The rejected summary drew one row a label: the word, a separate horizontal
bar as long as its share of the largest count, and the count.

## Change

The summary is the report page's meter (one bar, a cell a label in its
tone, sized by count) transposed for the drawer:

- One vertical bar, 6 points wide, dropping from under the attachment's page
  glyph (its centre 20 points in, where [[BPR-2331]]'s rule hung). A segment
  a count, in the report's order, in its label's tone (the theme's
  `report_verdict_*` roles; a neutral label in the window's ink at 45%),
  separated by the meter's 2 point gap. The bar's two ends are round, the
  joins square; each segment is filled as one path, so a translucent tone is
  even.
- A segment's band is one line of the current font plus its share, by count,
  of as much again for the whole summary: every label has its line, a larger
  count stands taller, equal counts stand equally tall. The bands are
  measured from the font at every layout, so they follow the scale and a
  larger font.
- Beside each segment, centred on it, its word in its tone and its count in
  semibold.
- A band (segment and label) is one button, as a row was: hover wash, focus
  outline, tooltip "Open the report at its first …", accessible name
  "nitpick: 1"; a press or Space opens the report at the first item it
  counts. A band never asks for more width than the bar, its word and its
  count, so the drawer never pans because of it.

`bpr::qt::report_tally_view` / `report_tally_row`
(`src/adapters/qt/include/bpr/qt/report_tally_view.hpp`); nothing outside the
view changed. `docs/architecture/ai-agent-boundary.md` describes the bar.

## Acceptance

- AC1. The summary is one bar: each count's segment has the bar's width and
  the same left edge, follows the segment above with only the meter's gap,
  and the bands make the whole bar.
- AC2. Sized by count: a larger count's segment is taller; equal counts'
  are equal; every segment is at least a line tall.
- AC3. Beside each segment its word and count, whole, apart, right of the
  bar and centred on their own segment; the summary fits the narrowest drawer
  (280) and a wide one (640), at 1.0, 1.25 and 2.0 scale and at a 1.6 times
  font.
- AC4. Pressing a segment, pressing a label, or Space on a focused band opens
  the report at that label's first finding; a band takes focus and names its
  count.
- AC5. `architecture_boundaries`, the format check and the documentation
  gate pass.

## Decisions

- The frontend-design skill was loaded for the bar. The design is the
  report page's own meter turned on its side, not a new chart: its tones,
  its 2 point cell gap, its order. No axis, no legend: the labels are the
  legend.
- Sizing: strictly proportional segments would make a 1 beside a 30 too
  short for its label, so each band is a line plus its share of a pool of
  one line a band. The proportion is kept in order and in equality, not in
  exact ratio; the exact counts are printed beside each segment.
- The bar replaces [[BPR-2331]]'s thin rule: it hangs where the rule hung,
  so the summary still reads as the attachment's.
- The constants `least_bar`/`preferred_bar` and the rows' bar track went
  with the horizontal bars; `bar_width`, `label_gap`, `line_height`,
  `band_height`, `bar_height` and `total` replaced them, and `segment_rect`
  replaced `bar_rect`.

## Verification

macOS arm64, dev preset (Debug with `-gline-tables-only`), a task tree of
its own on main `cb8a83eee` plus this change. `format_check.py`
(clang-format 22.1.8) on every changed C++ file.

| Criterion | Check | Result |
|---|---|---|
| AC1, AC2, AC3 | `qt_agent_report_summary_scale_100/125/200` (`expect_whole` at 280 and 640 points and a 1.6 times font: one bar, contiguous segments, bands summing to the bar, issue 2 taller than each 1, the 1s equal, labels whole and beside their own segment, the summary inside the drawer) | pass |
| AC4 | `qt_agent_report_summary_scale_*` (a row's click, a mouse press and release on the praise segment, Space on the focused nitpick band, each opening the report at that count's first row; `section_top` within 24 points) | pass |
| Not broken | `qt_agent_tone_colours` (the issue band wears Needs work), `application_review_report_public_path`, `application_follow_up_public_path` (the fixture agent's review shows the summary), `qt_workbench_window_scale_100` | pass |
| AC5 | `architecture_boundaries`, `format_check.py`, `tidy.py` on `report_tally_view.cpp` (clean), `python3 scripts/verify_documentation.py --root .` (`DOCUMENTATION_OK`) | pass |

Looks: offscreen renders at 280 and 640 points (`BPR_REPORT_SUMMARY_RENDER`),
viewed and discarded. No owner look was prepared in this round; the owner's
judgement of the new bar is not recorded here.

## Owner acceptance

The owner accepted this change on 2026-11-20 by looking directly at main
`c27fe51fe` (no attended-review session), with one nit: "the scroll when
clicking on something to jump to it isn't aligned properly, the thing we're
jumping to is cut off by the tab header." The nit is [[BPR-2338]].
